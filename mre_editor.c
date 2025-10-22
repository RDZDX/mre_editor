#include "vmsys.h"
#include "vmio.h"
#include "vmgraph.h"
#include "vmeditor.h"
#include "vmchset.h"
#include "vmstdlib.h"
#include "stdint.h"
#include <string.h>
#include <time.h>

static VMINT layer_hdl[1];

static VMINT32 editor = -1;
static VMUWCHAR buffer[1024];
static VMWCHAR initial_text[128];
static VMWCHAR left_label[32];
static VMWCHAR right_label[32];
static VMWCHAR center_label[32]; 
VMINT s_width = 0;
VMINT s_height = 0;
VMINT editor_height = 0;
VMINT response = -1;
VMBOOL trigeris = VM_FALSE;
VMBOOL trigeris1 = VM_FALSE;
vm_editor_font_attribute my_font = {0, 0, 0, 18}; //16

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

void vm_main(void) {
    layer_hdl[0] = -1;
    vm_reg_sysevt_callback(handle_sysevt);
    vm_ascii_to_ucs2(initial_text, sizeof(initial_text), "Hello");
    vm_ascii_to_ucs2(left_label, sizeof(left_label), "Exit"); // Open
    vm_ascii_to_ucs2(center_label, sizeof(center_label), "OK");
    vm_ascii_to_ucs2(right_label, sizeof(right_label), "Open"); // Back
    s_width = vm_graphic_get_screen_width();
    s_height = vm_graphic_get_screen_height();
    editor_height = s_height - vm_editor_get_softkey_height();
}

void handle_sysevt(VMINT message, VMINT param) {
    switch(message) {
    case VM_MSG_CREATE:
    case VM_MSG_ACTIVE:
        vm_switch_power_saving_mode(turn_off_mode);
        if(layer_hdl[0] < 0) {
            layer_hdl[0] = vm_graphic_create_layer(0, 0, s_width, s_height, -1);
            //layer_hdl[0] = vm_graphic_create_layer(0, 0, s_width, editor_height, -1);
            vm_graphic_set_clip(0,0,s_width, s_height);
        }
        if(editor <= 0) {

            //editor = vm_editor_create(VM_EDITOR_MULTILINE, 0, 0, s_width, s_height, buffer, sizeof(buffer), VM_FALSE, layer_hdl[0]);
            editor = vm_editor_create(VM_EDITOR_MULTILINE, 0, 0, s_width, editor_height, buffer, sizeof(buffer), VM_FALSE, layer_hdl[0]);
            vm_editor_set_multiline_text_font(editor, my_font);
            //vm_editor_set_IME(editor, VM_INPUT_TYPE_SENTENCE, NULL, 0, ime_cb);
            //VM_INPUT_TYPE_MULTITAP_SENTENCE
            //vm_editor_set_IME(editor, VM_INPUT_TYPE_SENTENCE, my_input_modes_lower_first, VM_INPUT_MODE_123, ime_cb);
            vm_editor_set_IME(editor, VM_INPUT_TYPE_SENTENCE, my_input_modes_lower_first, VM_INPUT_MODE_MULTITAP_FIRST_UPPERCASE_ABC, ime_cb);
            //vm_editor_set_IME(editor, VM_INPUT_TYPE_SENTENCE, my_input_modes_lower_first, VM_INPUT_MODE_MULTITAP_FIRST_UPPERCASE_RUSSIAN, ime_cb);
            //vm_editor_set_IME(editor, VM_INPUT_TYPE_USER_SPECIFIC, my_input_modes_lower_first, VM_INPUT_MODE_MULTITAP_FIRST_UPPERCASE_RUSSIAN, ime_cb);
            //vm_editor_set_text(editor, initial_text, sizeof(initial_text));
            vm_editor_set_update_text_callback(editor, my_vm_update_text_funcptr_cb);
            vm_editor_set_softkey(editor, left_label, VM_LEFT_SOFTKEY, close_and_exit); //change_label_name
            vm_editor_set_softkey(editor, center_label, VM_CENTER_SOFTKEY, ok_cr);
            vm_editor_set_softkey(editor, right_label, VM_RIGHT_SOFTKEY, change_label_name); //close_and_exit
            vm_editor_activate(editor, VM_TRUE);
        }
        //vm_graphic_flush_layer(layer_hdl, 1);
        break;

    case VM_MSG_PAINT:
        //if(editor>0) {vm_editor_show(editor);}
        vm_editor_show(editor);
        //vm_editor_activate(editor, VM_TRUE);
        //vm_graphic_flush_layer(layer_hdl, 1);
        vm_editor_redraw_ime_screen();
        break;

    case VM_MSG_INACTIVE:
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
    vm_exit_app();
}

void change_label_name(void) {

    if (response == -1) {
       response = vm_selector_run(0, 0, job);
       response = -1;
    } else {
       close_and_exit();
    }

}

static VMINT job(VMWCHAR *filepath, VMINT wlen) {

    response = 0;
    //vm_ascii_to_ucs2(left_label, sizeof(left_label), "Exit");
    //vm_editor_set_softkey(editor, left_label, VM_LEFT_SOFTKEY, close_and_exit);

    //vm_ascii_to_ucs2(right_label, sizeof(right_label), "Exit");
    //vm_editor_set_softkey(editor, right_label, VM_RIGHT_SOFTKEY, close_and_exit);

    vm_ascii_to_ucs2(left_label, sizeof(left_label), "Exit");
    vm_editor_set_softkey(editor, left_label, VM_LEFT_SOFTKEY, close_and_exit);

    //vm_editor_redraw_ime_screen();

    VMFILE f_read;
    VMUINT nread;
    VMUINT file_size;
    char *file_data = NULL;

    f_read = vm_file_open(filepath, MODE_READ, FALSE);
    if (f_read < 0) {return -1;}

    if (vm_file_getfilesize(f_read, &file_size) < 0) {
        vm_file_close(f_read);
        return -1;
    }

    file_data = (char *)vm_malloc(file_size);
    if (!file_data) {
        vm_file_close(f_read);
        return -1;
    }

    vm_file_read(f_read, file_data, file_size, &nread);
    file_data[nread] = '\0';

    vm_file_close(f_read);
    //if (file_size != nread) {
    //    vm_free(file_data);
    //    return -1;
    //}

    vm_ascii_to_ucs2(buffer, sizeof(buffer), file_data);
    //vm_chset_convert(VM_CHSET_UTF8, VM_CHSET_UCS2, file_data, (VMSTR)buffer, 4 * (strlen(file_data) + 1));

    vm_editor_set_text(editor, buffer, sizeof(buffer));
    //vm_editor_save(editor);
    //vm_editor_restore(editor);
    vm_free(file_data);

    return 0;

}

static void save_text_to_file() {

   VMINT drv = vm_get_system_driver();
   if (drv < 0) {return;}
   VMCHAR path[64];
   VMCHAR ascii_text[1024];
   sprintf(path, "%c:\text.txt", (VMCHAR)drv);
   VMWCHAR wpath[64];
   vm_ascii_to_ucs2(wpath, sizeof(wpath), path);

   vm_ucs2_to_ascii(ascii_text, sizeof(ascii_text), buffer);

   //vm_ucs2_to_ascii(CellNameStatusZ, sizeof(CellNameStatusZ), CellNameStatus);
   //vm_chset_convert(VM_CHSET_UCS2, VM_CHSET_UTF8, (VMSTR)CellNameStatus, CellNameStatusZ, sizeof(CellNameStatusZ));

   VMFILE f = vm_file_open(wpath, MODE_CREATE_ALWAYS_WRITE, TRUE);
   if (f >= 0) {
      VMUINT written = 0;
      vm_file_write(f, ascii_text, strlen(ascii_text), &written); // strlen ?????
      vm_file_close(f);
   }
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

    //if (editor <= 0 || layer_hdl[0] == -1) return;

    VMWCHAR tmp[1024];

    //memset(tmp, 0, sizeof(tmp));
    vm_editor_get_text(editor, (VMUWSTR)tmp, sizeof(tmp)); /* VMUWSTR required */

    int len = vm_wstrlen(tmp);

    if (len == 0){
       trigeris = VM_FALSE;
       response = -1;
       vm_ascii_to_ucs2(left_label, sizeof(left_label), "Exit");
       vm_editor_set_softkey(editor, left_label, VM_LEFT_SOFTKEY, close_and_exit);
    }

    if (len > 0 && trigeris == VM_FALSE){
       trigeris = VM_TRUE;
       vm_ascii_to_ucs2(left_label, sizeof(left_label), "Save");
       vm_editor_set_softkey(editor, left_label, VM_LEFT_SOFTKEY, close_and_exit);
       response = 0;
       //vm_editor_redraw_ime_screen();
    }

    VMCHAR ascii_buf[16];
    sprintf(ascii_buf, "%d", len);
    vm_ascii_to_ucs2(center_label, sizeof(center_label), ascii_buf);
    vm_editor_set_softkey(editor, center_label, VM_CENTER_SOFTKEY, ok_cr);

    vm_editor_redraw_ime_screen();

}