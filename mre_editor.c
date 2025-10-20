/* Updated full-screen editor with program layer enabled and auto-open + Back softkey */
#include "vmsys.h"
#include "vmio.h"
#include "vmgraph.h"
#include "vmeditor.h"
#include "vmchset.h"
#include "vmstdlib.h"

static VMINT layer = -1;
static VMINT32 editor = -1;
static VMWCHAR buffer[512];
static VMWCHAR initial_text[128];
static VMWCHAR left_label[32];
static VMWCHAR right_label[32];
static VMWCHAR center_label[32];

void handle_sysevt(VMINT message, VMINT param);
VMUINT32 ime_cb(VMINT32 h, vm_editor_message_struct_p m);
void close_and_exit(void);

void vm_main(void) {
    vm_reg_sysevt_callback(handle_sysevt);
    vm_ascii_to_ucs2(initial_text, sizeof(initial_text), "Hello");
    vm_ascii_to_ucs2(left_label, sizeof(left_label), "Start");
    vm_ascii_to_ucs2(center_label, sizeof(center_label), "OK");
    vm_ascii_to_ucs2(right_label, sizeof(right_label), "Back");
}

void handle_sysevt(VMINT message, VMINT param) {
    switch(message) {
    case VM_MSG_CREATE:
    case VM_MSG_ACTIVE:
        if(layer < 0) {
            layer = vm_graphic_create_layer(0,0,vm_graphic_get_screen_width(),vm_graphic_get_screen_height() - vm_editor_get_softkey_height(),-1);
            vm_graphic_set_clip(0,0,vm_graphic_get_screen_width(),vm_graphic_get_screen_height());
        }
        if(editor <= 0) {
            editor = vm_editor_create(VM_EDITOR_MULTILINE,0,0,
               vm_graphic_get_screen_width(),vm_graphic_get_screen_height(),
               buffer,sizeof(buffer),VM_FALSE,layer);
            vm_editor_set_IME(editor,VM_INPUT_TYPE_SENTENCE,NULL,0,ime_cb);
            vm_editor_set_text(editor,initial_text,sizeof(initial_text));
            vm_editor_set_softkey(editor,left_label,VM_LEFT_SOFTKEY,close_and_exit);
            vm_editor_set_softkey(editor,center_label,VM_CENTER_SOFTKEY,close_and_exit);
            vm_editor_set_softkey(editor,right_label,VM_RIGHT_SOFTKEY,close_and_exit);
            vm_editor_show(editor);
            vm_editor_activate(editor, VM_TRUE);
            vm_editor_redraw_ime_screen();
        }
        vm_graphic_flush_layer(&layer,1);
        break;
    case VM_MSG_PAINT:
        if(editor>0) vm_editor_show(editor);
        vm_graphic_flush_layer(&layer,1);
        break;
    case VM_MSG_QUIT:
        close_and_exit();
        break;
    }
}

VMUINT32 ime_cb(VMINT32 h, vm_editor_message_struct_p m) {
    vm_graphic_flush_layer(&layer,1);
    return 0;
}

void close_and_exit(void) {
    if(editor>0) { vm_editor_deactivate(editor); vm_editor_close(editor); editor=-1; }
    if(layer>=0) { vm_graphic_delete_layer(layer); layer=-1; }
    vm_exit_app();
}
