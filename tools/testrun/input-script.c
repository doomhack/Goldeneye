/* Minimal mupen64plus input plugin for headless test runs.
 *
 * The test runner (m64run.py) loads this plugin into its own process and sets
 * each controller's state once per VI through ScriptSetPad(). GetKeys() just
 * returns that state, so inputs are timed in VIs rather than in controller polls.
 *
 * Build: cc -shared -fPIC -O2 -o input-script.so input-script.c
 */
#include <stdlib.h>

#define EXPORT __attribute__((visibility("default")))

typedef struct { int Present; int RawData; int Plugin; } CONTROL;
typedef struct { CONTROL *Controls; } CONTROL_INFO;
typedef union { unsigned int Value; } BUTTONS;

static unsigned int padState[4];
static int padCount = 1;

/* value = buttons in bits 0-15, stick x in bits 16-23, stick y in bits 24-31 */
EXPORT void ScriptSetPad(int ctrl, unsigned int value)
{
    if (ctrl >= 0 && ctrl < 4)
        padState[ctrl] = value;
}

EXPORT void ScriptSetPadCount(int count)
{
    padCount = count;
}

EXPORT int PluginStartup(void *handle, void *context, void *debugCallback) { return 0; }
EXPORT int PluginShutdown(void) { return 0; }

EXPORT int PluginGetVersion(int *type, int *version, int *apiVersion, const char **name, int *caps)
{
    if (type) *type = 4; /* M64PLUGIN_INPUT */
    if (version) *version = 0x010000;
    if (apiVersion) *apiVersion = 0x020100;
    if (name) *name = "input-script";
    if (caps) *caps = 0;
    return 0;
}

EXPORT void InitiateControllers(CONTROL_INFO info)
{
    for (int i = 0; i < 4; i++) {
        info.Controls[i].Present = i < padCount;
        info.Controls[i].RawData = 0;
        info.Controls[i].Plugin = 1; /* PLUGIN_NONE: no controller pak */
    }
}

EXPORT void GetKeys(int ctrl, BUTTONS *keys)
{
    keys->Value = (ctrl >= 0 && ctrl < 4) ? padState[ctrl] : 0;
}

EXPORT void ControllerCommand(int ctrl, unsigned char *command) {}
EXPORT void ReadController(int ctrl, unsigned char *command) {}
EXPORT int RomOpen(void) { return 1; }
EXPORT void RomClosed(void) {}
EXPORT void SDL_KeyDown(int keymod, int keysym) {}
EXPORT void SDL_KeyUp(int keymod, int keysym) {}
