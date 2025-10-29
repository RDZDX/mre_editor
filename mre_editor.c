#include "vmsys.h"
#include "vmio.h"
#include "vmgraph.h"
#include "vmeditor.h"
#include "vmchset.h"
#include "vmstdlib.h"
#include "stdint.h"
#include <string.h>
#include <time.h>

#define READ_CHUNK_BYTES    2048                      /* read UTF-8 file in 2048-byte chunks (user-approved) */
#define VM_CHSET_DEST_BYTES 4096                      /* vm_chset_convert target max bytes (<=4096 per spec) */
#define UCS2_CHUNK_WORDS    (VM_CHSET_DEST_BYTES / 2) /* 2048 words */
#define INITIAL_UCS2_BYTES  (4096)                    /* initial dynamic buffer bytes (2048 words) */
#define MAX_UCS2_BYTES      (1500 * 1024)             /* maximum allowed editor buffer bytes (1500 KB) */ //(~3MB UCS2) 1UCS2 = 2xASCII
//#define MAX_UCS2_BYTES    (128 * 1024)              /* maximum allowed editor buffer bytes (128 KB) */  //(~256KB UCS2),

static VMINT layer_hdl[1];

static VMINT32 editor = -1; //editor  =  (RETURNS) =  NULL : Failed to create editor;  Other : If successful. -1 ???
static VMWCHAR left_label[32];
static VMWCHAR right_label[32];
static VMWCHAR center_label[32]; 
VMINT s_width = 0;
VMINT s_height = 0;
VMINT editor_height = 0;
VMINT response = -1;
VMBOOL trigeris = VM_FALSE;
VMBOOL trigeris1 = VM_FALSE;
VMBOOL trigeris2 = VM_FALSE;
vm_editor_font_attribute my_font = {0, 0, 0, 18}; //16
VMWCHAR outfile[100] = {0};
VMWCHAR bakfile[100] = {0};
static VMUWSTR ucs2_buf = NULL;    /* pointer to allocated UCS2 buffer (bytes) */
static VMINT ucs2_buf_bytes = 0;   /* allocated bytes */
static VMINT ucs2_len_words = 0;   /* used length in words (VMWCHAR) */
static vm_input_mode_enum my_input_modes_lower_first[] = {VM_INPUT_MODE_MULTITAP_FIRST_UPPERCASE_ABC, VM_INPUT_MODE_123, VM_INPUT_MODE_123_SYMBOLS, VM_INPUT_MODE_NONE};

VMBOOL trigerisx1 = VM_FALSE;
VMBOOL trigerisx2 = VM_FALSE;

void handle_sysevt(VMINT message, VMINT param);
VMUINT32 ime_cb(VMINT32 h, vm_editor_message_struct_p m);
void close_and_exit(void);
void change_label_name();
static VMINT job(VMWCHAR *filepath, VMINT wlen);
static void save_text_to_file();
void ok_cr(void);
void create_auto_filename(VMWSTR text);
void my_vm_update_text_funcptr_cb(VMUINT8 *text, VMUINT8 *cursor, VMINT32 text_length);
static void generate_output_filename(VMWSTR output_wpath, VMWSTR input_wpath);
static VMINT ensure_ucs2_capacity_bytes(VMINT needed_bytes);
static VMINT append_ucs2_words(VMWSTR src, VMINT src_words);
static void ucs2_buf_init_empty(void);
static VMINT load_utf8_file_to_ucs2_chunked(VMWSTR file_path);
static VMINT save_ucs2_buffer_to_utf8_chunked(VMWSTR outfile_path);
static void update_text_cb(VMUINT8 *text, VMUINT8 *cursor, VMINT32 text_length);
static void update_center_label_from_buffer(void);
VMINT app_load_file_into_editor(VMWSTR inpath);
VMINT app_save_editor_to_file(VMWSTR outpath);
static void softkey_center_cb(void);
static VMINT grow_and_attach_editor_buffer(VMINT new_bytes);
static void check_autoload_txt_file(void);
void create_selfapp_txt_filename(VMWSTR text, VMSTR extt);

void vm_main(void) {

    layer_hdl[0] = -1;
    vm_reg_sysevt_callback(handle_sysevt);
    vm_ascii_to_ucs2(left_label, sizeof(left_label), "Exit"); // Open
    vm_ascii_to_ucs2(center_label, sizeof(center_label), "OK");
    vm_ascii_to_ucs2(right_label, sizeof(right_label), "Open"); // Back
    s_width = vm_graphic_get_screen_width();
    s_height = vm_graphic_get_screen_height();
    editor_height = s_height - vm_editor_get_softkey_height();
    ucs2_buf_init_empty(); // ucs2_buf = INITIAL_UCS2_BYTES = (4096)
    check_autoload_txt_file();

}

void handle_sysevt(VMINT message, VMINT param) {

    switch(message) {
    case VM_MSG_CREATE:
    case VM_MSG_ACTIVE:
        vm_switch_power_saving_mode(turn_off_mode);
        if(layer_hdl[0] < 0) {
            layer_hdl[0] = vm_graphic_create_layer(0, 0, s_width, s_height, -1);
            vm_graphic_set_clip(0,0,s_width, s_height);
        }
        if(editor <= 0) {
            if (!ucs2_buf) ensure_ucs2_capacity_bytes(INITIAL_UCS2_BYTES); // jei ucs2_buf tuscias darom ucs2_buf = INITIAL_UCS2_BYTES = (2048)
            editor = vm_editor_create(VM_EDITOR_MULTILINE, 0, 0, s_width, editor_height, ucs2_buf, ucs2_buf_bytes, VM_FALSE, layer_hdl[0]);
            vm_editor_set_multiline_text_font(editor, my_font);
            vm_editor_set_IME(editor, VM_INPUT_TYPE_SENTENCE, my_input_modes_lower_first, VM_INPUT_MODE_MULTITAP_FIRST_UPPERCASE_ABC, ime_cb);
            vm_editor_set_update_text_callback(editor, my_vm_update_text_funcptr_cb);
            vm_editor_set_softkey(editor, left_label, VM_LEFT_SOFTKEY, close_and_exit); //change_label_name
            vm_editor_set_softkey(editor, center_label, VM_CENTER_SOFTKEY, ok_cr);
            vm_editor_set_softkey(editor, right_label, VM_RIGHT_SOFTKEY, change_label_name); //close_and_exit
            vm_editor_activate(editor, VM_TRUE);
            if(trigeris2 == VM_TRUE) {
               trigeris2 = VM_FALSE;
               app_load_file_into_editor(outfile);
            }
            //vm_editor_restore(editor);
        }
        break;

    case VM_MSG_PAINT:

        vm_editor_show(editor);
        vm_editor_redraw_ime_screen();
        break;

    case VM_MSG_INACTIVE:
        vm_editor_save(editor);
        vm_editor_deactivate(editor);
        if (layer_hdl[0] != -1 ) {
            vm_graphic_delete_layer(layer_hdl[0]);
            layer_hdl[0] = -1;
        }
        vm_switch_power_saving_mode(turn_on_mode);
        break;	

    case VM_MSG_QUIT:
        //if (layer_hdl[0] != -1) {
        //    vm_graphic_delete_layer(layer_hdl[0]);
        //    layer_hdl[0] = -1;
        //}

        if (ucs2_buf) {
            vm_free(ucs2_buf);
            ucs2_buf = NULL;
            ucs2_buf_bytes = 0;
            ucs2_len_words = 0;
        }

        close_and_exit();
        break;
    }
}

VMUINT32 ime_cb(VMINT32 h, vm_editor_message_struct_p m) {

    if (!m) return 0;
    switch (m->message_id) {
        case VM_EDITOR_MESSAGE_ACTIVATE:
        case VM_EDITOR_MESSAGE_REDRAW_FLOATING_UI:
        case VM_EDITOR_MESSAGE_REDRAW_IMUI_RECTANGLE:
            //update_center_label_from_buffer();
            if (editor > 0) {
                //vm_editor_show(editor);
                //vm_editor_redraw_ime_screen();
            }
            break;
        case VM_EDITOR_MESSAGE_DEACTIVATE:
            break;
        default:
            break;
    }
    return 0;
}

void close_and_exit(void) {

    vm_editor_deactivate(editor);
    vm_editor_close(editor);
    editor = -1;

    if (layer_hdl[0] != -1) {
        vm_graphic_delete_layer(layer_hdl[0]);
        layer_hdl[0] = -1;
    }
    if (ucs2_buf) {
        vm_free(ucs2_buf);
        ucs2_buf = NULL;
        ucs2_buf_bytes = 0;
        ucs2_len_words = 0;
    }

    vm_exit_app();
}

void change_label_name(void) {

    response = vm_selector_run(0, 0, job);
    //response = -1;

}

static VMINT job(VMWCHAR *filepath, VMINT wlen) {

    //response = -1;
    trigeris1 = VM_TRUE;

    vm_ascii_to_ucs2(left_label, sizeof(left_label), "Exit");
    vm_editor_set_softkey(editor, left_label, VM_LEFT_SOFTKEY, close_and_exit);
    vm_wstrcpy(outfile, filepath);
    generate_output_filename(bakfile, filepath);
    app_load_file_into_editor(filepath);

    return 0;

}

static void save_text_to_file() {

   //vm_editor_get_text(editor, ucs2_buf, ucs2_buf_bytes); //ar reikia ???
   //ucs2_len_words = vm_wstrlen(ucs2_buf);  // -> "my_vm_update_text_funcptr_cb"
   if(trigeris1 == VM_FALSE) {
      create_auto_filename(outfile);
   } else if (trigeris1 == VM_TRUE && response == 0){
      vm_file_rename(outfile, bakfile);
   } else {}
   save_ucs2_buffer_to_utf8_chunked(outfile);
   close_and_exit();
}


void ok_cr(void) {

    VMINT32 len = vm_editor_get_cursor_index(editor);

    VMCHAR ascii_buf[16];
    sprintf(ascii_buf, "%d", len);
    vm_ascii_to_ucs2(center_label, sizeof(center_label), ascii_buf);
    vm_editor_set_softkey(editor, center_label, VM_CENTER_SOFTKEY, ok_cr);
    vm_editor_redraw_ime_screen();

}

void create_auto_filename(VMWSTR text) {

    VMINT drv;
    VMCHAR fAutoFileName[100] = {0};
    VMCHAR fData_text[100] = {0};
    VMWCHAR wAutoFileName[100] = {0};
    struct vm_time_t curr_time;

    vm_get_time(&curr_time);

    if ((drv = vm_get_removable_driver()) < 0) {
       drv = vm_get_system_driver();
    }

    sprintf(fAutoFileName, "%c:\\", drv);
    sprintf(fData_text, "%02d%02d%02d%02d%02d.txt", curr_time.mon, curr_time.day, curr_time.hour, curr_time.min, curr_time.sec);
    strcat(fAutoFileName, fData_text);
    vm_ascii_to_ucs2(text, (strlen(fAutoFileName) + 1) * 2, fAutoFileName);
}

void my_vm_update_text_funcptr_cb(VMUINT8 *text, VMUINT8 *cursor, VMINT32 text_length)
{

    ucs2_len_words = text_length;

    VMINT free_words = (ucs2_buf_bytes / sizeof(VMWCHAR)) - text_length - 1;
    if (free_words < 100) {
        VMINT want_bytes = ucs2_buf_bytes ? ucs2_buf_bytes * 2 : INITIAL_UCS2_BYTES;
        if (want_bytes > MAX_UCS2_BYTES) want_bytes = MAX_UCS2_BYTES;
        if (want_bytes > ucs2_buf_bytes) {
            if (grow_and_attach_editor_buffer(want_bytes) == 0) {
            } else {
                //to low space, stop editing, err msg
            }
        }
    }

    if (trigeris == VM_FALSE) {
        trigeris = VM_TRUE;
        vm_ascii_to_ucs2(left_label, sizeof(left_label), "Save");
        vm_editor_set_softkey(editor, left_label, VM_LEFT_SOFTKEY, save_text_to_file);
        //response = 0;
    }

    if (text_length == 0) {
        trigeris = VM_FALSE;
        trigeris1 = VM_FALSE;
        //response = -1;
        vm_ascii_to_ucs2(left_label, sizeof(left_label), "Exit");
        vm_editor_set_softkey(editor, left_label, VM_LEFT_SOFTKEY, close_and_exit);
    }

    VMCHAR ascii_buf[16];
    sprintf(ascii_buf, "%d", text_length);
    vm_ascii_to_ucs2(center_label, sizeof(center_label), ascii_buf);
    vm_editor_set_softkey(editor, center_label, VM_CENTER_SOFTKEY, ok_cr);
    vm_editor_redraw_ime_screen();
}

static void generate_output_filename(VMWSTR output_wpath, VMWSTR input_wpath) {

    VMCHAR path_ascii[300];
    VMCHAR result_ascii[300];
    int i, last_dot = -1;
    struct vm_time_t curr_time;

    vm_get_time(&curr_time);

    vm_ucs2_to_ascii(path_ascii, sizeof(path_ascii), input_wpath);

    for(i = 0; path_ascii[i]; i++)
        if(path_ascii[i] == '.') last_dot = i;

    if(last_dot < 0) {
        //sprintf(result_ascii, "%s_txt.bak", path_ascii);
        sprintf(result_ascii, "%s%02d%02d%02d%02d%02d_txt.bak", path_ascii, curr_time.mon, curr_time.day, curr_time.hour, curr_time.min, curr_time.sec);
    } else {
        path_ascii[last_dot] = '\0';
        //sprintf(result_ascii, "%s_%s.bak", path_ascii, path_ascii + last_dot + 1);
        sprintf(result_ascii, "%s%02d%02d%02d%02d%02d_%s.bak", path_ascii, curr_time.mon, curr_time.day, curr_time.hour, curr_time.min, curr_time.sec, path_ascii + last_dot + 1);
    }
    vm_ascii_to_ucs2(output_wpath, sizeof(result_ascii) * 2, result_ascii);
}

static VMINT ensure_ucs2_capacity_bytes(VMINT needed_bytes)
{
    if (needed_bytes <= ucs2_buf_bytes) return 0;
    if (needed_bytes > MAX_UCS2_BYTES) return -1;
    VMINT new_bytes = ucs2_buf_bytes ? ucs2_buf_bytes * 2 : INITIAL_UCS2_BYTES;
    while (new_bytes < needed_bytes) {
        new_bytes *= 2;
        if (new_bytes > MAX_UCS2_BYTES) { new_bytes = MAX_UCS2_BYTES; break; }
    }
    if (new_bytes > MAX_UCS2_BYTES) new_bytes = MAX_UCS2_BYTES;
    if (new_bytes < needed_bytes) return -1;

    if (ucs2_buf == NULL) {
        ucs2_buf = (VMUWSTR)vm_malloc(new_bytes);
        if (!ucs2_buf) {
            ucs2_buf_bytes = 0;
            return -1;
        }
    } else {
        void *p = NULL;
        p = vm_realloc(ucs2_buf, new_bytes);
        if (!p) return -1;
        ucs2_buf = (VMUWSTR)p;
    }
    ucs2_buf_bytes = new_bytes;
    return 0;
}

static VMINT append_ucs2_words(VMWSTR src, VMINT src_words)
{
    if (!src || src_words <= 0) return 0;

    VMINT needed_bytes = (ucs2_len_words + src_words + 1) * sizeof(VMWCHAR);
    if (needed_bytes > MAX_UCS2_BYTES) return -1;

    if (ensure_ucs2_capacity_bytes(needed_bytes) < 0) return -1;

    VMINT free_words = (ucs2_buf_bytes / sizeof(VMWCHAR)) - ucs2_len_words;
    if (free_words <= 0) return -1;

    VMINT copied = vm_safe_wstrcpy(ucs2_buf + ucs2_len_words, free_words, src);

    ucs2_len_words += copied;

    return copied;
}

static void ucs2_buf_init_empty(void)
{
    if (ucs2_buf) {
        vm_free(ucs2_buf);
        ucs2_buf = NULL;
    }

    ucs2_buf_bytes = 0;
    ucs2_len_words = 0;
    ensure_ucs2_capacity_bytes(INITIAL_UCS2_BYTES);
    if (ucs2_buf) ucs2_buf[0] = 0;
}

static VMINT load_utf8_file_to_ucs2_chunked(VMWSTR file_path)
{
    if (!file_path) return -1;
    VMFILE fh = vm_file_open(file_path, MODE_READ, TRUE);  //TRUE FALSE
    if (fh < 0) return -1;

    uint8_t *read_buf = (uint8_t *)vm_calloc(READ_CHUNK_BYTES + 8); /* +8 safety */
    VMWCHAR *temp_ucs2 = (VMWCHAR *)vm_calloc((UCS2_CHUNK_WORDS + 4) * sizeof(VMWCHAR));
    if (!read_buf || !temp_ucs2) {
        if (read_buf) vm_free(read_buf);
        if (temp_ucs2) vm_free(temp_ucs2);
        vm_file_close(fh);
        return -1;
    }

    ucs2_len_words = 0;
    VMUINT nread = 0;
    while (!vm_file_is_eof(fh)) {
        VMINT rc = vm_file_read(fh, read_buf, READ_CHUNK_BYTES, &nread);
        if (rc <= 0 && !vm_file_is_eof(fh)) {
            vm_free(read_buf);
            vm_free(temp_ucs2);
            vm_file_close(fh);
            return -2;
        }
        if (nread == 0) break;

        read_buf[nread] = 0;

        VMINT conv = vm_chset_convert(VM_CHSET_UTF8, VM_CHSET_UCS2, (VMCHAR *)read_buf, (VMCHAR *)temp_ucs2, VM_CHSET_DEST_BYTES);
        if (conv != VM_CHSET_CONVERT_SUCCESS && conv != 0) {
            vm_free(read_buf);
            vm_free(temp_ucs2);
            vm_file_close(fh);
            return -3;
        }

        VMINT words = vm_wstrlen(temp_ucs2);
        if (words > 0) {
            if (append_ucs2_words(temp_ucs2, words) < 0) {
                vm_free(read_buf);
                vm_free(temp_ucs2);
                vm_file_close(fh);
                return -4; /* out of space */
            }
        }
    }

    vm_free(read_buf);
    vm_free(temp_ucs2);
    vm_file_close(fh);
    return ucs2_len_words;
}

static VMINT save_ucs2_buffer_to_utf8_chunked(VMWSTR outfile_path)
{
    if (!outfile_path) return -1;
    VMFILE fh = vm_file_open(outfile_path, MODE_CREATE_ALWAYS_WRITE, FALSE); //TRUE FALSE
    if (fh < 0) return -2;

    VMWCHAR *u_chunk = (VMWCHAR *)vm_calloc((UCS2_CHUNK_WORDS + 4) * sizeof(VMWCHAR));
    char *out_utf8 = (char *)vm_calloc(VM_CHSET_DEST_BYTES + 8);
    if (!u_chunk || !out_utf8) {
        if (u_chunk) vm_free(u_chunk);
        if (out_utf8) vm_free(out_utf8);
        vm_file_close(fh);
        return -3;
    }

    VMINT remaining = ucs2_len_words;
    VMINT pos = 0;
    while (remaining > 0) {
        VMINT take = remaining > UCS2_CHUNK_WORDS ? UCS2_CHUNK_WORDS : remaining;
        vm_wstrncpy(u_chunk, ucs2_buf + pos, take);
        u_chunk[take] = 0;

        VMINT conv = vm_chset_convert(VM_CHSET_UCS2, VM_CHSET_UTF8, (VMCHAR *)u_chunk, (VMCHAR *)out_utf8, VM_CHSET_DEST_BYTES);
        if (conv != VM_CHSET_CONVERT_SUCCESS && conv != 0) {
            vm_free(u_chunk);
            vm_free(out_utf8);
            vm_file_close(fh);
            return -4;
        }

        VMUINT written = 0;
        vm_file_write(fh, out_utf8, (VMUINT)strlen((VMSTR)out_utf8), &written);

        pos += take;
        remaining -= take;
    }

    vm_free(u_chunk);
    vm_free(out_utf8);
    vm_file_close(fh);
    return 0;
}

static void update_center_label_from_buffer(void)
{

    VMINT32 len = vm_editor_get_cursor_index(editor);

    VMCHAR ascii_buf[16];
    sprintf(ascii_buf, "%d", len);
    vm_ascii_to_ucs2(center_label, sizeof(center_label), ascii_buf);
    vm_editor_set_softkey(editor, center_label, VM_CENTER_SOFTKEY, ok_cr);
    vm_editor_redraw_ime_screen();

}

static void update_text_cb(VMUINT8 *text, VMUINT8 *cursor, VMINT32 text_length)
{
    update_center_label_from_buffer();
}

VMINT app_load_file_into_editor(VMWSTR inpath)
{
    if (!inpath) return -1;
    ucs2_buf_init_empty();
    VMINT res = load_utf8_file_to_ucs2_chunked(inpath);
    if (res < 0) return res;
    vm_editor_set_text(editor, ucs2_buf, ucs2_buf_bytes);
    update_center_label_from_buffer();

    return res;
}

VMINT app_save_editor_to_file(VMWSTR outpath)
{
    if (!outpath) return -1;
    return save_ucs2_buffer_to_utf8_chunked(outpath);
}

static void softkey_center_cb(void)
{

}

static VMINT grow_and_attach_editor_buffer(VMINT new_bytes)
{
    if (new_bytes <= ucs2_buf_bytes) return 0;
    if (new_bytes > MAX_UCS2_BYTES) return -1;

    VMUWSTR new_buf = (VMUWSTR)vm_malloc(new_bytes);
    if (!new_buf) return -1;
    vm_editor_get_text(editor, new_buf, new_bytes);
    vm_editor_set_text(editor, new_buf, new_bytes);
    if (ucs2_buf) {
        vm_free(ucs2_buf);
    }
    ucs2_buf = new_buf;
    ucs2_buf_bytes = new_bytes;
    ucs2_len_words = vm_wstrlen(ucs2_buf); // ??? -> "my_vm_update_text_funcptr_cb"

    return 0;
}

static void check_autoload_txt_file(void) {

        VMFILE f_read;

        create_selfapp_txt_filename(outfile, "txt");
	f_read = vm_file_open(outfile, MODE_READ, FALSE);

	if (f_read < 0) {
           vm_file_close(f_read);
           //trigeris2 = VM_FALSE;
	} else {
           vm_file_close(f_read);
           //app_load_file_into_editor(outfile);
           trigeris1 = VM_TRUE;
	   trigeris2 = VM_TRUE;
	}
}

void create_selfapp_txt_filename(VMWSTR text, VMSTR extt) {

    VMWCHAR fullPath[100] = {0};
    VMWCHAR wfile_extension[8] = {0};

    vm_get_exec_filename(fullPath);
    vm_ascii_to_ucs2(wfile_extension, 8, extt);
    vm_wstrncpy(text, fullPath, vm_wstrlen(fullPath) - 3);
    vm_wstrcat(text, wfile_extension);

}
