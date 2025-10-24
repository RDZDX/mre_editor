#include "vmsys.h"
#include "vmio.h"
#include "vmgraph.h"
#include "vmeditor.h"
#include "vmchset.h"
#include "vmstdlib.h"
#include "stdint.h"
#include <string.h>
#include <time.h>

#define READ_CHUNK_BYTES      2048           /* read UTF-8 file in 2048-byte chunks (user-approved) */
#define VM_CHSET_DEST_BYTES   4096           /* vm_chset_convert target max bytes (<=4096 per spec) */
#define UCS2_CHUNK_WORDS      (VM_CHSET_DEST_BYTES / 2) /* 2048 words */
#define INITIAL_UCS2_BYTES    (4096)         /* initial dynamic buffer bytes (2048 words) */
#define MAX_UCS2_BYTES        (512 * 1024)   /* maximum allowed editor buffer bytes (512 KB) */

static VMINT layer_hdl[1];

static VMINT32 editor = -1;
static VMUWCHAR buffer[2048]; //buffer[700000] (~1.4MB UCS2),
//static VMWCHAR initial_text[128];
static VMWCHAR left_label[32];
static VMWCHAR right_label[32];
static VMWCHAR center_label[32]; 

VMWCHAR init_small[64];

VMINT s_width = 0;
VMINT s_height = 0;
VMINT editor_height = 0;
VMINT response = -1;
VMBOOL trigeris = VM_FALSE;
VMBOOL trigeris1 = VM_FALSE;
vm_editor_font_attribute my_font = {0, 0, 0, 18}; //16
VMWCHAR outfile[100];

static VMINT layer_hdl_arr[1] = {-1};
/* dynamic UCS2 buffer: VMUWSTR (VMUINT16 *) used by editor */
static VMUWSTR ucs2_buf = NULL;    /* pointer to allocated UCS2 buffer (bytes) */
static VMINT ucs2_buf_bytes = 0;   /* allocated bytes */
static VMINT ucs2_len_words = 0;   /* used length in words (VMWCHAR) */


static vm_input_mode_enum my_input_modes_lower_first[] = {VM_INPUT_MODE_MULTITAP_FIRST_UPPERCASE_ABC, VM_INPUT_MODE_123, VM_INPUT_MODE_123_SYMBOLS, VM_INPUT_MODE_NONE};
//static vm_input_mode_enum my_input_modes_lower_first[] = {VM_INPUT_MODE_MULTITAP_FIRST_UPPERCASE_ABC, VM_INPUT_MODE_MULTITAP_FIRST_UPPERCASE_LITHUANIAN, VM_INPUT_MODE_MULTITAP_FIRST_UPPERCASE_RUSSIAN, VM_INPUT_MODE_NONE};

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
static VMUINT32 ime_cb1(VMINT32 h, vm_editor_message_struct_p m);
static void softkey_left_cb1(void);
static void update_text_cb(VMUINT8 *text, VMUINT8 *cursor, VMINT32 text_length);
static void update_center_label_from_buffer(void);
VMINT app_load_file_into_editor(VMWSTR inpath);
VMINT app_save_editor_to_file(VMWSTR outpath);
static void softkey_center_cb(void);

void vm_main(void) {
    layer_hdl[0] = -1;
    vm_reg_sysevt_callback(handle_sysevt);
    //vm_ascii_to_ucs2(initial_text, sizeof(initial_text), "Hello");
    vm_ascii_to_ucs2(left_label, sizeof(left_label), "Exit"); // Open
    vm_ascii_to_ucs2(center_label, sizeof(center_label), "OK");
    vm_ascii_to_ucs2(right_label, sizeof(right_label), "Open"); // Back
    s_width = vm_graphic_get_screen_width();
    s_height = vm_graphic_get_screen_height();
    editor_height = s_height - vm_editor_get_softkey_height();
    ucs2_buf_init_empty();

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
            if (!ucs2_buf) ensure_ucs2_capacity_bytes(INITIAL_UCS2_BYTES);
            //editor = vm_editor_create(VM_EDITOR_MULTILINE, 0, 0, s_width, editor_height, buffer, sizeof(buffer), VM_FALSE, layer_hdl[0]);
            editor = vm_editor_create(VM_EDITOR_MULTILINE, 0, 0, s_width, editor_height, ucs2_buf, ucs2_buf_bytes, VM_FALSE, layer_hdl[0]);

            vm_editor_set_multiline_text_font(editor, my_font);
            //vm_editor_set_IME(editor, VM_INPUT_TYPE_SENTENCE, NULL, 0, ime_cb);
            //VM_INPUT_TYPE_MULTITAP_SENTENCE
            //vm_editor_set_IME(editor, VM_INPUT_TYPE_SENTENCE, my_input_modes_lower_first, VM_INPUT_MODE_123, ime_cb);
            vm_editor_set_IME(editor, VM_INPUT_TYPE_SENTENCE, my_input_modes_lower_first, VM_INPUT_MODE_MULTITAP_FIRST_UPPERCASE_ABC, ime_cb);
            //vm_editor_set_IME(editor, VM_INPUT_TYPE_SENTENCE, my_input_modes_lower_first, VM_INPUT_MODE_MULTITAP_FIRST_UPPERCASE_RUSSIAN, ime_cb);
            //vm_editor_set_IME(editor, VM_INPUT_TYPE_USER_SPECIFIC, my_input_modes_lower_first, VM_INPUT_MODE_MULTITAP_FIRST_UPPERCASE_RUSSIAN, ime_cb);
            //vm_editor_set_text(editor, initial_text, sizeof(initial_text));


            vm_editor_set_update_text_callback(editor, my_vm_update_text_funcptr_cb);
            //vm_editor_set_update_text_callback(editor, update_text_cb);


            vm_editor_set_softkey(editor, left_label, VM_LEFT_SOFTKEY, close_and_exit); //change_label_name
            vm_editor_set_softkey(editor, center_label, VM_CENTER_SOFTKEY, ok_cr);
            vm_editor_set_softkey(editor, right_label, VM_RIGHT_SOFTKEY, change_label_name); //close_and_exit
            vm_editor_activate(editor, VM_TRUE);
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

    //VMWCHAR tmp[1024];
    //memset(tmp, 0, sizeof(tmp));
    //vm_editor_get_text(editor, (VMUWSTR)tmp, sizeof(tmp)); /* VMUWSTR required */
    //int len = vm_wstrlen(tmp);

    //VMCHAR ascii_buf[16];
    //sprintf(ascii_buf, "%d", len);  // ascii_buf = "3" pvz.
    //vm_ascii_to_ucs2(left_label, sizeof(left_label), ascii_buf);
    //vm_editor_set_softkey(editor, left_label, VM_LEFT_SOFTKEY, close_and_exit);
    //vm_editor_show(editor);

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

    //if (response == -1) {
       response = vm_selector_run(0, 0, job);
       response = -1;
    //} else {
       //close_and_exit();
    //}

}

static VMINT job(VMWCHAR *filepath, VMINT wlen) {

    response = 0;
    trigeris1 = VM_TRUE;

    vm_ascii_to_ucs2(left_label, sizeof(left_label), "Exit");
    vm_editor_set_softkey(editor, left_label, VM_LEFT_SOFTKEY, close_and_exit);

    generate_output_filename(outfile, filepath);

    //VMFILE f_read;
    //VMUINT nread;
    //VMUINT file_size;
    //char *file_data = NULL;

    //f_read = vm_file_open(filepath, MODE_READ, FALSE);
    //if (f_read < 0) {return -1;}

    //if (vm_file_getfilesize(f_read, &file_size) < 0) {
    //    vm_file_close(f_read);
    //    return -1;
    //}

    //file_data = (char *)vm_malloc(file_size);
    //if (!file_data) {
    //    vm_file_close(f_read);
    //    return -1;
    //}

    //vm_file_read(f_read, file_data, file_size, &nread);
    //file_data[nread] = '\0';

    //vm_file_close(f_read);
    ////if (file_size != nread) {
    ////    vm_free(file_data);
    ////    return -1;
    ////}

    //vm_ascii_to_ucs2(buffer, sizeof(buffer), file_data);
    //vm_chset_convert(VM_CHSET_UTF8, VM_CHSET_UCS2, file_data, (VMSTR)buffer, 4 * (strlen(file_data) + 1));
    //vm_chset_convert(VM_CHSET_UTF8, VM_CHSET_UCS2, file_data, (VMSTR)buffer, sizeof(buffer)); // BAYTAIS! pvz. 1024 * 2 

    //vm_editor_set_text(editor, buffer, sizeof(buffer));
    //vm_free(file_data);

    app_load_file_into_editor(filepath);

    return 0;

}

static void save_text_to_file() {

   //VMCHAR ascii_text[1024];

   //vm_ucs2_to_ascii(ascii_text, sizeof(ascii_text), buffer);
   //vm_chset_convert(VM_CHSET_UCS2, VM_CHSET_UTF8, (VMSTR)buffer, ascii_text, sizeof(ascii_text));

   if(trigeris1 == VM_FALSE) {create_auto_filename(outfile);}

   //VMFILE f = vm_file_open(outfile, MODE_CREATE_ALWAYS_WRITE, FALSE);
   //if (f >= 0) {
   //   VMUINT written = 0;
   //   vm_file_write(f, ascii_text, strlen(ascii_text), &written); // strlen ?????
   //   vm_file_close(f);
   //}

   save_ucs2_buffer_to_utf8_chunked(outfile);

   close_and_exit();
}


void ok_cr(void) {

    //VMUWCHAR my_cr[6] = {0};
    //VMUWCHAR my_cr[4] = {0};
    //vm_ascii_to_ucs2(my_cr, sizeof(my_cr), "\r\n");
    //vm_ascii_to_ucs2(my_cr, sizeof(my_cr), "\n");
    //vm_editor_insert_string(editor, my_cr, 6);
    //vm_editor_insert_string(editor, my_cr, 4);
    //vm_editor_redraw_ime_screen();

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

void my_vm_update_text_funcptr_cb(VMUINT8 *text, VMUINT8 *cursor, VMINT32 text_length) {

    ////if (editor <= 0 || layer_hdl[0] == -1) return;

    //VMWCHAR tmp[1024];

    //memset(tmp, 0, sizeof(tmp));
    //vm_editor_get_text(editor, (VMUWSTR)tmp, sizeof(tmp)); /* VMUWSTR required */

    //int len = vm_wstrlen(tmp);

    //int len = ucs2_len_words;
    //VMINT32 len = vm_editor_get_cursor_index(editor);
    //len = kounteris;

    if (trigeris == VM_FALSE){
       trigeris = VM_TRUE;
       vm_ascii_to_ucs2(left_label, sizeof(left_label), "Save");
       vm_editor_set_softkey(editor, left_label, VM_LEFT_SOFTKEY, save_text_to_file);
       response = 0;
    }

    if (text_length == 0){
       trigeris = VM_FALSE;
       trigeris1 = VM_FALSE;
       response = -1;
       vm_ascii_to_ucs2(left_label, sizeof(left_label), "Exit");
       vm_editor_set_softkey(editor, left_label, VM_LEFT_SOFTKEY, close_and_exit);
    }

    VMCHAR ascii_buf[16];
    //sprintf(ascii_buf, "%d", len);
    sprintf(ascii_buf, "%d", text_length);
    vm_ascii_to_ucs2(center_label, sizeof(center_label), ascii_buf);
    vm_editor_set_softkey(editor, center_label, VM_CENTER_SOFTKEY, ok_cr);

    //update_center_label_from_buffer();

    vm_editor_redraw_ime_screen();

}

static void generate_output_filename(VMWSTR output_wpath, VMWSTR input_wpath) {

    VMCHAR path_ascii[300];
    VMCHAR result_ascii[300];
    int i, last_dot = -1;

    vm_ucs2_to_ascii(path_ascii, sizeof(path_ascii), input_wpath);

    for(i = 0; path_ascii[i]; i++)
        if(path_ascii[i] == '.') last_dot = i;

    if(last_dot < 0) {
        sprintf(result_ascii, "%s_txt.txt", path_ascii); // Nėra plėtinio → tiesiog pridedame "_txt.txt"
    } else {
        path_ascii[last_dot] = '\0';

        sprintf(result_ascii, "%s_%s.txt", path_ascii, path_ascii + last_dot + 1); // originalus vardas + "_" + senasis plėtinys + ".txt"
    }

    vm_ascii_to_ucs2(output_wpath, sizeof(result_ascii) * 2, result_ascii);
}

// Updates: auto expand buffer memmory loading big files, open/save conversion by 2048 chunks loading big files  
//--------------------------------------------------------------------------------------

/* helpers for dynamic buffer */
static VMINT ensure_ucs2_capacity_bytes(VMINT needed_bytes)
{
    if (needed_bytes <= ucs2_buf_bytes) return 0;
    if (needed_bytes > MAX_UCS2_BYTES) return -1; /* exceed maximum */
    /* grow strategy: double or add chunk */
    VMINT new_bytes = ucs2_buf_bytes ? ucs2_buf_bytes * 2 : INITIAL_UCS2_BYTES;
    while (new_bytes < needed_bytes) {
        new_bytes *= 2;
        if (new_bytes > MAX_UCS2_BYTES) { new_bytes = MAX_UCS2_BYTES; break; }
    }
    if (new_bytes > MAX_UCS2_BYTES) new_bytes = MAX_UCS2_BYTES;
    if (new_bytes < needed_bytes) return -1;

    if (ucs2_buf == NULL) {
        ucs2_buf = (VMUWSTR)vm_malloc(new_bytes);
        if (!ucs2_buf) { ucs2_buf_bytes = 0; return -1; }
    } else {
        void *p = vm_realloc(ucs2_buf, new_bytes);
        if (!p) return -1;
        ucs2_buf = (VMUWSTR)p;
    }
    ucs2_buf_bytes = new_bytes;
    return 0;
}

/* append UCS2 words (VMWCHAR) into dynamic buffer; src_words is number of VMWCHAR elements */
static VMINT append_ucs2_words(VMWSTR src, VMINT src_words)
{
    if (!src || src_words <= 0) return 0;
    VMINT needed_bytes = (ucs2_len_words + src_words + 1) * sizeof(VMWCHAR); /* +1 for null term */
    if (needed_bytes > MAX_UCS2_BYTES) return -1;
    if (ensure_ucs2_capacity_bytes(needed_bytes) < 0) return -1;
    /* copy */
    vm_wstrncpy(ucs2_buf + ucs2_len_words, src, src_words);
    ucs2_len_words += src_words;
    ucs2_buf[ucs2_len_words] = 0;
    return src_words;
}

/* Clear and initialize dynamic buffer */
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

/* Load UTF-8 file into UCS2 dynamic buffer using vm_chset_convert chunked.
   Returns >=0 words written, negative on error. */
static VMINT load_utf8_file_to_ucs2_chunked(VMWSTR file_path)
{
    if (!file_path) return -1;
    VMFILE fh = vm_file_open(file_path, MODE_READ, TRUE); //TRUE
    if (fh < 0) return -1;

    uint8_t *read_buf = (uint8_t *)vm_calloc(READ_CHUNK_BYTES + 8);
    VMWCHAR *temp_ucs2 = (VMWCHAR *)vm_calloc(UCS2_CHUNK_WORDS + 4); /* UCS2_CHUNK_WORDS words */
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
            vm_free(read_buf); vm_free(temp_ucs2); vm_file_close(fh);
            return -2;
        }
        if (nread == 0) break;

        /* Ensure zero-terminated input for vm_chset_convert */
        read_buf[nread] = 0;

        /* convert this UTF-8 chunk to UCS2 into temp_ucs2 (bytes <= VM_CHSET_DEST_BYTES) */
        VMINT conv = vm_chset_convert(VM_CHSET_UTF8, VM_CHSET_UCS2, (VMCHAR *)read_buf, (VMCHAR *)temp_ucs2, VM_CHSET_DEST_BYTES);
        if (conv != VM_CHSET_CONVERT_SUCCESS) {
            vm_free(read_buf); vm_free(temp_ucs2); vm_file_close(fh);
            return -3;
        }

        /* temp_ucs2 is UCS2 (VMWCHAR). Determine number of words */
        VMINT words = vm_wstrlen(temp_ucs2);
        if (words > 0) {
            if (append_ucs2_words(temp_ucs2, words) < 0) {
                vm_free(read_buf); vm_free(temp_ucs2); vm_file_close(fh);
                return -4; /* out of space */
            }
        }
    }

    /* cleanup */
    vm_free(read_buf);
    vm_free(temp_ucs2);
    vm_file_close(fh);
    return ucs2_len_words;
}

/* Save dynamic UCS2 buffer to UTF-8 file in chunks (no BOM).
   outfile_path is UCS2 path. Returns 0 on success, negative on error. */
static VMINT save_ucs2_buffer_to_utf8_chunked(VMWSTR outfile_path)
{
    if (!outfile_path) return -1;
    VMFILE fh = vm_file_open(outfile_path, MODE_CREATE_ALWAYS_WRITE, TRUE); //TRUE
    if (fh < 0) return -2;

    /* We'll convert UCS2 chunks of up to UCS2_CHUNK_WORDS words -> dest bytes VM_CHSET_DEST_BYTES */
    VMWCHAR *u_chunk = (VMWCHAR *)vm_calloc(UCS2_CHUNK_WORDS + 4);
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
        if (conv != VM_CHSET_CONVERT_SUCCESS) {
            vm_free(u_chunk); vm_free(out_utf8); vm_file_close(fh);
            return -4;
        }
        /* out_utf8 is zero-terminated C string */
        VMUINT written = 0;
        vm_file_write(fh, out_utf8, strlen((VMSTR)out_utf8), &written);
        pos += take;
        remaining -= take;
    }

    vm_free(u_chunk);
    vm_free(out_utf8);
    vm_file_close(fh);
    return 0;
}

/* update center softkey label to character count (number of UCS2 words) */
static void update_center_label_from_buffer(void)
{
    //int len = ucs2_len_words;
    //char ascii_count[32];
    //memset(ascii_count, 0, sizeof(ascii_count));
    //sprintf(ascii_count, "%d", len);
    //vm_ascii_to_ucs2(center_label, sizeof(center_label), ascii_count);
    //if (editor > 0) {
    //    vm_editor_set_softkey(editor, center_label, VM_CENTER_SOFTKEY, softkey_center_cb);
    //    /* flush editor layer so softkey redraw may be triggered */
    //    //if (layer_hdl_arr[0] != -1) vm_graphic_flush_layer(&layer_hdl_arr[0], 1);
    //    vm_editor_redraw_ime_screen();
    //}

    VMINT32 len = vm_editor_get_cursor_index(editor);

    VMCHAR ascii_buf[16];
    sprintf(ascii_buf, "%d", len);
    vm_ascii_to_ucs2(center_label, sizeof(center_label), ascii_buf);
    vm_editor_set_softkey(editor, center_label, VM_CENTER_SOFTKEY, ok_cr);
    vm_editor_redraw_ime_screen();

}

/* Called by vm_editor when text changes (before redraw). We'll schedule update of softkey text.
   Note: signature expects VMUINT8* for byte-based text, but we don't use it here; we rely on ucs2_buf. */
static void update_text_cb(VMUINT8 *text, VMUINT8 *cursor, VMINT32 text_length)
{
    /* text/content changed; update center label. This callback is synchronous. */
    update_center_label_from_buffer();
}

/* IME callback: handle floating UI redraws and activation changes */
static VMUINT32 ime_cb1(VMINT32 h, vm_editor_message_struct_p m)
{
    if (!m) return 0;
    switch (m->message_id) {
        case VM_EDITOR_MESSAGE_ACTIVATE:
        case VM_EDITOR_MESSAGE_REDRAW_FLOATING_UI:
        case VM_EDITOR_MESSAGE_REDRAW_IMUI_RECTANGLE:
            /* ensure editor redisplayed and softkeys updated */
            update_center_label_from_buffer();
            if (editor > 0) {
                vm_editor_show(editor);
                vm_editor_redraw_ime_screen();
            }
            if (layer_hdl_arr[0] != -1) vm_graphic_flush_layer(&layer_hdl_arr[0], 1);
            break;
        case VM_EDITOR_MESSAGE_DEACTIVATE:
            /* nothing special */
            break;
        default:
            break;
    }
    return 0;
}

/* Softkey callbacks */
static void softkey_left_cb1(void)   /* Save + exit */
{
    /* create output filename (auto) - here we use fixed path on system driver for demo */
    VMWCHAR outpath[260] = {0};
    /* Example: write to system driver root as out.txt (real app should generate unique name) */
    VMINT drv = vm_get_system_driver();
    if (drv < 0) drv = vm_get_removable_driver();
    if (drv < 0) drv = 'C';
    VMCHAR root[4] = {0};
    sprintf(root, "%c:\\", drv);
    vm_ascii_to_ucs2(outpath, sizeof(outpath), root);
    vm_wstrcat(outpath, (VMWCHAR *)vm_malloc(4)); /* placeholder to avoid compiler warnings */
    /* Properly build filename: keep simple "out.txt" appended */
    vm_ascii_to_ucs2(outpath + vm_wstrlen(outpath), sizeof(outpath) - vm_wstrlen(outpath)*2, "out.txt");

    /* Save */
    save_ucs2_buffer_to_utf8_chunked(outpath);

    /* Close & exit */
    if (editor > 0) {
        vm_editor_deactivate(editor);
        vm_editor_close(editor);
        editor = -1;
    }
    if (layer_hdl_arr[0] != -1) {
        vm_graphic_delete_layer(layer_hdl_arr[0]);
        layer_hdl_arr[0] = -1;
    }
    vm_editor_redraw_ime_screen();
    vm_exit_app();
}

/* Example API functions for external use (choose appropriate path-building in real app) */

/* load file (UCS2 path) into editor buffer and update editor text */
VMINT app_load_file_into_editor(VMWSTR inpath)
{
    if (!inpath) return -1;
    /* reset buffer */
    ucs2_buf_init_empty();
    VMINT res = load_utf8_file_to_ucs2_chunked(inpath);
    if (res < 0) return res;
    /* if editor exists, update its text pointer and content */
    //if (editor > 0) {
        /* ensure editor sees current buffer size in bytes */
        vm_editor_set_text(editor, ucs2_buf, ucs2_buf_bytes);
        //vm_editor_show(editor);
        //vm_editor_redraw_ime_screen();
        update_center_label_from_buffer();
        //if (layer_hdl_arr[0] != -1) vm_graphic_flush_layer(&layer_hdl_arr[0], 1);
    //}
    return res;
}

/* save current buffer to file (UCS2 path) */
VMINT app_save_editor_to_file(VMWSTR outpath)
{
    if (!outpath) return -1;
    return save_ucs2_buffer_to_utf8_chunked(outpath);
}

static void softkey_center_cb(void) /* center: could be used to show info - here does nothing */
{
    /* no-op or could show count; keep as no-op */
}