// SPDX-License-Identifier: AGPL-3.0-only

#include "credential_ime.h"
#include "credential.h"

#include <psp2/apputil.h>
#include <psp2/display.h>
#include <psp2/gxm.h>
#include <psp2/ime_dialog.h>
#include <psp2/kernel/sysmem.h>
#include <psp2/libime.h>

#include <stdint.h>
#include <string.h>

#define ALIGN_UP(value, alignment) (((value) + (alignment) - 1) & ~((alignment) - 1))
#define DISPLAY_WIDTH 960
#define DISPLAY_HEIGHT 544
#define DISPLAY_STRIDE 1024
#define DISPLAY_BUFFER_COUNT 2
#define DISPLAY_BYTES (4 * DISPLAY_STRIDE * DISPLAY_HEIGHT)

typedef struct display_buffer_t
{
	void *data;
	SceGxmSyncObject *sync;
	SceGxmColorSurface surface;
	SceUID uid;
	bool mapped;
} DisplayBuffer;

static DisplayBuffer display_buffers[DISPLAY_BUFFER_COUNT];
static unsigned int front_buffer;
static unsigned int back_buffer;
static SceDisplayFrameBuf saved_frame;
static bool display_visible, display_unusable;
static bool display_term(void);

static void secure_clear(void *value, size_t size)
{
	volatile unsigned char *p = value;
	while(size--)
		*p++ = 0;
}

static bool memory_is_clear(const void *value, size_t size)
{
	const unsigned char *p = value;
	while(size--)
		if(*p++)
			return false;
	return true;
}

static void display_callback(const void *callback_data)
{
	void *base = *(void *const *)callback_data;
	SceDisplayFrameBuf frame = {
		.size = sizeof(frame),
		.base = base,
		.pitch = DISPLAY_STRIDE,
		.pixelformat = 0,
		.width = DISPLAY_WIDTH,
		.height = DISPLAY_HEIGHT,
	};
	sceDisplaySetFrameBuf(&frame, SCE_DISPLAY_SETBUF_NEXTFRAME);
	display_visible = true;
}

static bool display_init(void)
{
	if(display_unusable) return false;
	front_buffer = 0;
	back_buffer = 1;
	display_visible = false;
	memset(&saved_frame, 0, sizeof(saved_frame));
	saved_frame.size = sizeof(saved_frame);
	if(sceDisplayGetFrameBuf(&saved_frame, SCE_DISPLAY_SETBUF_NEXTFRAME) < 0) saved_frame.base = NULL;
	SceGxmInitializeParams params = {
		.flags = 0,
		.displayQueueMaxPendingCount = 1,
		.displayQueueCallback = display_callback,
		.displayQueueCallbackDataSize = sizeof(void *),
		.parameterBufferSize = SCE_GXM_DEFAULT_PARAMETER_BUFFER_SIZE,
	};
	if(sceGxmInitialize(&params) < 0)
		return false;

	for(unsigned int i = 0; i < DISPLAY_BUFFER_COUNT; i++)
	{
		size_t allocation_size = ALIGN_UP(DISPLAY_BYTES, 256 * 1024);
		display_buffers[i].uid = sceKernelAllocMemBlock("plus_vita_ime",
			SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW, allocation_size, NULL);
		if(display_buffers[i].uid < 0
			|| sceKernelGetMemBlockBase(display_buffers[i].uid, &display_buffers[i].data) < 0
			|| sceGxmMapMemory(display_buffers[i].data, allocation_size,
				SCE_GXM_MEMORY_ATTRIB_READ | SCE_GXM_MEMORY_ATTRIB_WRITE) < 0) goto error;
		display_buffers[i].mapped = true;
		if(sceGxmColorSurfaceInit(&display_buffers[i].surface,
				SCE_GXM_COLOR_FORMAT_A8B8G8R8, SCE_GXM_COLOR_SURFACE_LINEAR,
				SCE_GXM_COLOR_SURFACE_SCALE_NONE, SCE_GXM_OUTPUT_REGISTER_SIZE_32BIT,
				DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_STRIDE,
				display_buffers[i].data) < 0
			|| sceGxmSyncObjectCreate(&display_buffers[i].sync) < 0)
			goto error;
	}
	return true;
error:
	display_term();
	return false;
}

static void display_swap(void)
{
	sceGxmPadHeartbeat(&display_buffers[back_buffer].surface,
		display_buffers[back_buffer].sync);
	sceGxmDisplayQueueAddEntry(display_buffers[front_buffer].sync,
		display_buffers[back_buffer].sync, &display_buffers[back_buffer].data);
	front_buffer = back_buffer;
	back_buffer = (back_buffer + 1) % DISPLAY_BUFFER_COUNT;
}

static bool display_term(void)
{
	sceGxmDisplayQueueFinish();
	if(display_visible && (sceDisplaySetFrameBuf(saved_frame.base ? &saved_frame : NULL,
		SCE_DISPLAY_SETBUF_NEXTFRAME) < 0 || sceDisplayWaitSetFrameBuf() < 0))
	{
		/* Keep active buffers alive rather than free memory still scanned out. */
		display_unusable = true;
		return false;
	}
	for(unsigned int i = 0; i < DISPLAY_BUFFER_COUNT; i++)
	{
		if(display_buffers[i].sync)
			sceGxmSyncObjectDestroy(display_buffers[i].sync);
		if(display_buffers[i].mapped)
			sceGxmUnmapMemory(display_buffers[i].data);
		if(display_buffers[i].uid >= 0)
			sceKernelFreeMemBlock(display_buffers[i].uid);
	}
	sceGxmTerminate();
	memset(display_buffers, 0, sizeof(display_buffers));
	return true;
}

static void credential_ime_read(PlusVitaCredential *credential,
	PlusVitaCredentialImeResult *result, bool masked, char *label, bool search)
{
	static const uint16_t title[] = {
		'N','P','S','S','O',' ','v','i','s','i','b','l','e',' ','-',' ','n','o','t',' ','s','a','v','e','d',0
	};
	static const uint16_t masked_title[] = { 'E','n','t','e','r',' ','N','P','S','S','O',0 };
	static const uint16_t label_title[] = { 'A','c','c','o','u','n','t',' ','l','a','b','e','l',0 };
	static const uint16_t search_title[] = { 'S','e','a','r','c','h',' ','g','a','m','e','s',0 };
	uint16_t input[SCE_IME_DIALOG_MAX_TEXT_LENGTH + 1] = { 0 };
	bool app_initialized = false;
	bool gxm_initialized = false;
	bool dialog_initialized = false;

	memset(result, 0, sizeof(*result));
	plus_vita_credential_init(credential);
	for(unsigned int i = 0; i < DISPLAY_BUFFER_COUNT; i++)
		display_buffers[i].uid = -1;

	SceAppUtilInitParam app_param = { 0 };
	SceAppUtilBootParam boot_param = { 0 };
	if(sceAppUtilInit(&app_param, &boot_param) < 0)
		goto cleanup;
	app_initialized = true;
	SceCommonDialogConfigParam common_param = { 0 };
	sceCommonDialogSetConfigParam(&common_param);
	if(!display_init())
		goto cleanup;
	gxm_initialized = true;

	SceImeDialogParam param;
	sceImeDialogParamInit(&param);
	param.supportedLanguages = label ? 0 : SCE_IME_LANGUAGE_ENGLISH;
	param.languagesForced = label ? SCE_FALSE : SCE_TRUE;
	param.type = label ? SCE_IME_TYPE_DEFAULT : SCE_IME_TYPE_BASIC_LATIN;
	param.option = SCE_IME_OPTION_NO_AUTO_CAPITALIZATION | SCE_IME_OPTION_NO_ASSISTANCE;
	param.dialogMode = SCE_IME_DIALOG_DIALOG_MODE_WITH_CANCEL;
	param.textBoxMode = masked ? SCE_IME_DIALOG_TEXTBOX_MODE_PASSWORD : SCE_IME_DIALOG_TEXTBOX_MODE_DEFAULT;
	param.title = label ? (search ? search_title : label_title) : masked ? masked_title : title;
	param.maxTextLength = label ? 32 : masked ? 64 : PLUS_VITA_CREDENTIAL_MAX;
	param.initialText = NULL;
	param.inputTextBuffer = input;
	if(sceImeDialogInit(&param) < 0)
		goto cleanup;
	dialog_initialized = true;
	result->initialized = true;

	while(sceImeDialogGetStatus() != SCE_COMMON_DIALOG_STATUS_FINISHED)
	{
		memset(display_buffers[back_buffer].data, 0, DISPLAY_BYTES);
		SceCommonDialogUpdateParam update = { 0 };
		update.renderTarget.colorFormat = SCE_GXM_COLOR_FORMAT_A8B8G8R8;
		update.renderTarget.surfaceType = SCE_GXM_COLOR_SURFACE_LINEAR;
		update.renderTarget.width = DISPLAY_WIDTH;
		update.renderTarget.height = DISPLAY_HEIGHT;
		update.renderTarget.strideInPixels = DISPLAY_STRIDE;
		update.renderTarget.colorSurfaceData = display_buffers[back_buffer].data;
		update.renderTarget.depthSurfaceData = NULL;
		update.displaySyncObject = display_buffers[back_buffer].sync;
		sceCommonDialogUpdate(&update);
		display_swap();
		sceDisplayWaitVblankStart();
	}

	SceImeDialogResult dialog_result = { 0 };
	sceImeDialogGetResult(&dialog_result);
	result->accepted = dialog_result.button == SCE_IME_DIALOG_BUTTON_ENTER;
	if(result->accepted && label)
	{
		size_t length = 0;
		bool valid = true;
		for(unsigned i = 0; valid && input[i]; i++)
		{
			uint32_t code = input[i];
			if(code >= 0xd800 && code <= 0xdbff) {
				uint32_t low = input[++i];
				if(low < 0xdc00 || low > 0xdfff) { valid = false; break; }
				code = 0x10000 + ((code - 0xd800) << 10) + low - 0xdc00;
			} else if(code >= 0xdc00 && code <= 0xdfff) { valid = false; break; }
			if(code < 32 || code == 127) { valid = false; break; }
			unsigned count = code < 0x80 ? 1 : code < 0x800 ? 2 : code < 0x10000 ? 3 : 4;
			if(length + count > 128) { valid = false; break; }
			if(count == 1) label[length++] = (char)code;
			else {
				label[length++] = (char)((count == 2 ? 0xc0 : count == 3 ? 0xe0 : 0xf0) | (code >> (6 * (count - 1))));
				for(unsigned j = count - 1; j > 0; j--) label[length++] = (char)(0x80 | ((code >> (6 * (j - 1))) & 63));
			}
		}
		label[length] = 0;
		result->entered_length = length;
		result->validated = valid;
	}
	else if(result->accepted)
	{
		char ascii[PLUS_VITA_CREDENTIAL_MAX + 1] = { 0 };
		size_t length = 0;
		while(length < PLUS_VITA_CREDENTIAL_MAX && input[length])
		{
			if(input[length] < 0x21 || input[length] > 0x7e)
				break;
			ascii[length] = (char)input[length];
			length++;
		}
		result->entered_length = length;
		result->validated = input[length] == 0
			&& plus_vita_credential_set(credential, ascii, length);
		secure_clear(ascii, sizeof(ascii));
	}

cleanup:
	if(dialog_initialized)
		sceImeDialogTerm();
	if(gxm_initialized && !display_term()) result->validated = false;
	if(!result->validated)
	{
		plus_vita_credential_clear(credential);
		if(label) secure_clear(label, 129);
	}
	result->credential_wiped = plus_vita_credential_is_clear(credential);
	secure_clear(input, sizeof(input));
	result->input_wiped = memory_is_clear(input, sizeof(input));
	if(app_initialized)
		sceAppUtilShutdown();
}

void plus_vita_credential_ime_read(PlusVitaCredential *credential, PlusVitaCredentialImeResult *result)
{
	credential_ime_read(credential, result, false, NULL, false);
}

void plus_vita_credential_ime_read_masked(PlusVitaCredential *credential, PlusVitaCredentialImeResult *result)
{
	credential_ime_read(credential, result, true, NULL, false);
}

void plus_vita_account_label_read(char output[129], PlusVitaCredentialImeResult *result)
{
	PlusVitaCredential unused;
	memset(output, 0, 129);
	credential_ime_read(&unused, result, false, output, false);
	plus_vita_credential_clear(&unused);
}

void plus_vita_catalogue_search_read(char output[129], PlusVitaCredentialImeResult *result)
{
	PlusVitaCredential unused;
	memset(output,0,129);
	credential_ime_read(&unused,result,false,output,true);
	plus_vita_credential_clear(&unused);
}
