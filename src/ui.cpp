#include "WaveEdit.hpp"

#ifdef ARCH_LIN
	#include <linux/limits.h>
#endif
#ifdef ARCH_MAC
	#include <sys/syslimits.h>
#endif
#include <libgen.h>

#include <SDL.h>
#include <SDL_opengl.h>

#include "imgui.h"
#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui_internal.h"

#include "osdialog/osdialog.h"

#include "lodepng/lodepng.h"

#include "tablabels.hpp"


static bool showTestWindow = false;
char lastFilename[1024] = "";
static int styleId = 0;
int selectedId = 0;
int lastSelectedId = 0;


static void refreshStyle();


enum Page {
	EDITOR_PAGE = 0,
	EFFECT_PAGE,
	GRID_PAGE,
	WATERFALL_PAGE,
	IMPORT_PAGE,
	NUM_PAGES
};

Page currentPage = EDITOR_PAGE;


static ImVec4 lighten(ImVec4 col, float p) {
	col.x = crossf(col.x, 1.0, p);
	col.y = crossf(col.y, 1.0, p);
	col.z = crossf(col.z, 1.0, p);
	col.w = crossf(col.w, 1.0, p);
	return col;
}

static ImVec4 darken(ImVec4 col, float p) {
	col.x = crossf(col.x, 0.0, p);
	col.y = crossf(col.y, 0.0, p);
	col.z = crossf(col.z, 0.0, p);
	col.w = crossf(col.w, 0.0, p);
	return col;
}

static ImVec4 alpha(ImVec4 col, float a) {
	col.w *= a;
	return col;
}


static ImTextureID loadImage(const char *filename) {
	GLuint textureId;
	glGenTextures(1, &textureId);
	glBindTexture(GL_TEXTURE_2D, textureId);
	unsigned char *pixels;
	unsigned int width, height;
	unsigned err = lodepng_decode_file(&pixels, &width, &height, filename, LCT_RGBA, 8);
	assert(!err);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
	glBindTexture(GL_TEXTURE_2D, 0);
	return (void*)(intptr_t) textureId;
}


static void getImageSize(ImTextureID id, int *width, int *height) {
	GLuint textureId = (GLuint)(intptr_t) id;
	glBindTexture(GL_TEXTURE_2D, textureId);
	glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, width);
	glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, height);
	glBindTexture(GL_TEXTURE_2D, 0);
}


static void selectWave(int waveId) {
	selectedId = waveId;
	lastSelectedId = selectedId;
	morphX = (float)(selectedId % BANK_GRID_WIDTH);
	morphY = (float)(selectedId / BANK_GRID_WIDTH);
	morphZ = (float)selectedId;
}


static void refreshMorphSnap() {
	if (!morphInterpolate && morphZSpeed <= 0.f) {
		morphX = roundf(morphX);
		morphY = roundf(morphY);
		morphZ = roundf(morphZ);
	}
}

/** Focuses to a page which displays the current bank, useful when loading a new bank and showing the user some visual feedback that the bank has changed. */
static void showCurrentBankPage() {
	switch (currentPage) {
		case EFFECT_PAGE:
		case IMPORT_PAGE:
			currentPage = EDITOR_PAGE;
			break;
		default:
			break;
	}
}

/* static void menuAbout() {
	openBrowser("about.pdf");
} */

static void menuNewBank() {
	showCurrentBankPage();
	currentBank.clear();
	lastFilename[0] = '\0';
	historyPush();
}

/** Caller must free() return value, guaranteed to not be NULL */
static char *getLastDir() {
	if (lastFilename[0] == '\0') {
		return strdup(".");
	}
	else {
		char filename[PATH_MAX];
		strncpy(filename, lastFilename, sizeof(filename));
		char *dir = dirname(filename);
		return strdup(dir);
	}
}

static void menuOpenBank() {
	char *dir = getLastDir();
	char *path = osdialog_file(OSDIALOG_OPEN, dir, NULL, NULL);
	if (path) {
		showCurrentBankPage();
		currentBank.loadWAV(path);
		snprintf(lastFilename, sizeof(lastFilename), "%s", path);
		historyPush();
		free(path);
	}
	free(dir);
}

static void menuSaveBankAs() {
	char *dir = getLastDir();
	char *path = osdialog_file(OSDIALOG_SAVE, dir, "Untitled.wav", NULL);
	if (path) {
		currentBank.saveWAV(path);
		snprintf(lastFilename, sizeof(lastFilename), "%s", path);
		free(path);
	}
	free(dir);
}

static void menuSaveBank() {
	if (lastFilename[0] != '\0')
		currentBank.saveWAV(lastFilename);
	else
		menuSaveBankAs();
}

static void menuSaveWaves() {
	char *dir = getLastDir();
	char *path = osdialog_file(OSDIALOG_OPEN_DIR, dir, NULL, NULL);
	if (path) {
		currentBank.saveWaves(path);
		free(path);
	}
	free(dir);
}

static void menuQuit() {
	SDL_Event event;
	event.type = SDL_QUIT;
	SDL_PushEvent(&event);
}

static void menuSelectAll() {
	selectedId = 0;
	lastSelectedId = BANK_LEN-1;
}

static void menuCopy() {
	currentBank.waves[selectedId].clipboardCopy();
}

static void menuCut() {
	currentBank.waves[selectedId].clipboardCopy();
	currentBank.waves[selectedId].clear();
	historyPush();
}

static void menuPaste() {
	currentBank.waves[selectedId].clipboardPaste();
	historyPush();
}

static void menuClear() {
	for (int i = mini(selectedId, lastSelectedId); i <= maxi(selectedId, lastSelectedId); i++) {
		currentBank.waves[i].clear();
	}
	historyPush();
}

static void menuRandomize() {
	for (int i = mini(selectedId, lastSelectedId); i <= maxi(selectedId, lastSelectedId); i++) {
		currentBank.waves[i].randomizeEffects();
	}
	historyPush();
}

static void incrementSelectedId(int delta) {
	selectWave(clampi(selectedId + delta, 0, BANK_LEN-1));
}

static void menuKeyCommands() {
	ImGuiContext &g = *GImGui;
	ImGuiIO &io = ImGui::GetIO();

	if (io.OSXBehaviors ? io.KeySuper : io.KeyCtrl) {
		if (ImGui::IsKeyPressed(SDLK_n) && !io.KeyShift && !io.KeyAlt)
			menuNewBank();
		if (ImGui::IsKeyPressed(SDLK_o) && !io.KeyShift && !io.KeyAlt)
			menuOpenBank();
		if (ImGui::IsKeyPressed(SDLK_s) && !io.KeyShift && !io.KeyAlt)
			menuSaveBank();
		if (ImGui::IsKeyPressed(SDLK_s) && io.KeyShift && !io.KeyAlt)
			menuSaveBankAs();
		if (ImGui::IsKeyPressed(SDLK_q) && !io.KeyShift && !io.KeyAlt)
			menuQuit();
		if (ImGui::IsKeyPressed(SDLK_z) && !io.KeyShift && !io.KeyAlt)
			historyUndo();
		if (ImGui::IsKeyPressed(SDLK_z) && io.KeyShift && !io.KeyAlt)
			historyRedo();
		if (ImGui::IsKeyPressed(SDLK_a) && !io.KeyShift && !io.KeyAlt)
			menuSelectAll();
		if (ImGui::IsKeyPressed(SDLK_c) && !io.KeyShift && !io.KeyAlt)
			menuCopy();
		if (ImGui::IsKeyPressed(SDLK_x) && !io.KeyShift && !io.KeyAlt)
			menuCut();
		if (ImGui::IsKeyPressed(SDLK_v) && !io.KeyShift && !io.KeyAlt)
			menuPaste();
	}
	// I have NO idea why the scancode is needed here but the keycodes are needed for the letters.
	// It looks like SDLZ_F1 is not defined correctly or something.
	/* if (ImGui::IsKeyPressed(SDL_SCANCODE_F1))
		menuManual(); */

	if (!io.KeySuper && !io.KeyCtrl && !io.KeyShift && !io.KeyAlt) {
		// Only trigger these key commands if no text box is focused
		if (!g.ActiveId || g.ActiveId != GImGui->InputTextState.Id) {
			if (ImGui::IsKeyPressed(SDLK_r))
				menuRandomize();
			if (ImGui::IsKeyPressed(io.OSXBehaviors ? SDLK_BACKSPACE : SDLK_DELETE))
				menuClear();
			// Pages
			if (ImGui::IsKeyPressed(SDLK_SPACE))
				playEnabled = !playEnabled;
			if (ImGui::IsKeyPressed(SDLK_1))
				currentPage = EDITOR_PAGE;
			if (ImGui::IsKeyPressed(SDLK_2))
				currentPage = EFFECT_PAGE;
			if (ImGui::IsKeyPressed(SDLK_3))
				currentPage = GRID_PAGE;
			if (ImGui::IsKeyPressed(SDLK_4))
				currentPage = WATERFALL_PAGE;
			if (ImGui::IsKeyPressed(SDLK_5))
				currentPage = IMPORT_PAGE;
			if (ImGui::IsKeyPressed(SDL_SCANCODE_UP))
				incrementSelectedId(currentPage == GRID_PAGE ? -BANK_GRID_WIDTH : -1);
			if (ImGui::IsKeyPressed(SDL_SCANCODE_DOWN))
				incrementSelectedId(currentPage == GRID_PAGE ? BANK_GRID_WIDTH : 1);
			if (ImGui::IsKeyPressed(SDL_SCANCODE_LEFT))
				incrementSelectedId(-1);
			if (ImGui::IsKeyPressed(SDL_SCANCODE_RIGHT))
				incrementSelectedId(1);
		}
	}
}

void renderWaveMenu() {
	char menuName[128];
	int selectedStart = mini(selectedId, lastSelectedId);
	int selectedEnd = maxi(selectedId, lastSelectedId);
	if (selectedStart != selectedEnd)
		snprintf(menuName, sizeof(menuName), "(Wave %d to %d)", selectedStart, selectedEnd);
	else
		snprintf(menuName, sizeof(menuName), "(Wave %d)", selectedId);
	ImGui::MenuItem(menuName, NULL, false, false);

	if (ImGui::MenuItem("Clear", "Delete")) {
		menuClear();
	}
	if (ImGui::MenuItem("Randomize Effects", "R")) {
		menuRandomize();
	}

	if (selectedStart != selectedEnd) {
		ImGui::MenuItem("##spacer3", NULL, false, false);
		snprintf(menuName, sizeof(menuName), "(Wave %d)", selectedId);
		ImGui::MenuItem(menuName, NULL, false, false);
	}

	if (ImGui::MenuItem("Copy", ImGui::GetIO().OSXBehaviors ? "Cmd+C" : "Ctrl+C")) {
		menuCopy();
	}
	if (ImGui::MenuItem("Cut", ImGui::GetIO().OSXBehaviors ? "Cmd+X" : "Ctrl+X")) {
		menuCut();
	}
	if (ImGui::MenuItem("Paste", ImGui::GetIO().OSXBehaviors ? "Cmd+V" : "Ctrl+V", false, clipboardActive)) {
		menuPaste();
	}

	if (ImGui::MenuItem("Open Wave...")) {
		char *dir = getLastDir();
		char *path = osdialog_file(OSDIALOG_OPEN, dir, NULL, NULL);
		if (path) {
			currentBank.waves[selectedId].loadWAV(path);
			historyPush();
			snprintf(lastFilename, sizeof(lastFilename), "%s", path);
			free(path);
		}
		free(dir);
	}
	if (ImGui::MenuItem("Save Wave As...")) {
		char *dir = getLastDir();
		char *path = osdialog_file(OSDIALOG_SAVE, dir, "Untitled.wav", NULL);
		if (path) {
			currentBank.waves[selectedId].saveWAV(path);
			snprintf(lastFilename, sizeof(lastFilename), "%s", path);
			free(path);
		}
		free(dir);
	}
}

void renderMenu() {
	menuKeyCommands();

	// Draw main menu
	if (ImGui::BeginMenuBar()) {
		// File
		if (ImGui::BeginMenu("File")) {
			if (ImGui::MenuItem("New Bank", ImGui::GetIO().OSXBehaviors ? "Cmd+N" : "Ctrl+N"))
				menuNewBank();
			if (ImGui::MenuItem("Open Bank...", ImGui::GetIO().OSXBehaviors ? "Cmd+O" : "Ctrl+O"))
				menuOpenBank();
			if (ImGui::MenuItem("Save Bank", ImGui::GetIO().OSXBehaviors ? "Cmd+S" : "Ctrl+S"))
				menuSaveBank();
			if (ImGui::MenuItem("Save Bank As...", ImGui::GetIO().OSXBehaviors ? "Cmd+Shift+S" : "Ctrl+Shift+S"))
				menuSaveBankAs();
			if (ImGui::MenuItem("Save Waves to Folder...", NULL))
				menuSaveWaves();
			if (ImGui::MenuItem("Quit", ImGui::GetIO().OSXBehaviors ? "Cmd+Q" : "Ctrl+Q"))
				menuQuit();

			ImGui::EndMenu();
		}
		// Edit
		if (ImGui::BeginMenu("Edit")) {
			if (ImGui::MenuItem("Undo", ImGui::GetIO().OSXBehaviors ? "Cmd+Z" : "Ctrl+Z"))
				historyUndo();
			if (ImGui::MenuItem("Redo", ImGui::GetIO().OSXBehaviors ? "Cmd+Shift+Z" : "Ctrl+Shift+Z"))
				historyRedo();
			if (ImGui::MenuItem("Select All", ImGui::GetIO().OSXBehaviors ? "Cmd+A" : "Ctrl+A"))
				menuSelectAll();
			ImGui::MenuItem("##spacer", NULL, false, false);
			renderWaveMenu();
			ImGui::EndMenu();
		}
		// Audio Output
		if (ImGui::BeginMenu("Audio Output")) {
			int deviceCount = audioGetDeviceCount();
			for (int deviceId = 0; deviceId < deviceCount; deviceId++) {
				const char *deviceName = audioGetDeviceName(deviceId);
				if (ImGui::MenuItem(deviceName, NULL, false)) audioOpen(deviceId);
			}
			ImGui::EndMenu();
		}
		// Colors
		if (ImGui::BeginMenu("Colors")) {
            if (ImGui::MenuItem("Monsta Dark", NULL, styleId == 0)) {
				styleId = 0;
				refreshStyle();
			}
			if (ImGui::MenuItem("Monsta Light", NULL, styleId == 1)) {
				styleId = 1;
				refreshStyle();
			}
			ImGui::EndMenu();
		}
		/* Help
		if (ImGui::BeginMenu("Help")) {
			if (ImGui::MenuItem("Manual PDF", "F1", false))
				menuManual();
			if (ImGui::MenuItem("About PDF", NULL, false))
				menuAbout();
			// if (ImGui::MenuItem("imgui Demo", NULL, showTestWindow)) showTestWindow = !showTestWindow;
			ImGui::EndMenu();
		} */
		ImGui::EndMenuBar();
	}
}


void renderPreview() {
	ImGui::Checkbox("Play", &playEnabled);
	ImGui::SameLine();
	ImGui::PushItemWidth(300.0);
	ImGui::SliderFloat("##playVolume", &playVolume, -60.0f, 0.0f, "Volume: %.2f dB");
	ImGui::PushItemWidth(-1.0);
	ImGui::SameLine();
	ImGui::SliderFloat("##playFrequency", &playFrequency, 1.0f, 10000.0f, "Frequency: %.2f Hz", 0.0f);

	ImGui::Checkbox("Morph Interpolate", &morphInterpolate);
	if (playModeXY) {
		ImGui::SameLine();
		ImGui::PushItemWidth(-1.0);
		float width = ImGui::CalcItemWidth() / 2.0 - ImGui::GetStyle().FramePadding.y;
		ImGui::PushItemWidth(width);
		ImGui::SliderFloat("##Morph X", &morphX, 0.0, BANK_GRID_WIDTH - 1, "Morph X: %.3f");
		ImGui::SameLine();
		ImGui::SliderFloat("##Morph Y", &morphY, 0.0, BANK_GRID_HEIGHT - 1, "Morph Y: %.3f");
	}
	else {
		ImGui::SameLine();
		ImGui::PushItemWidth(-1.0);
		float width = ImGui::CalcItemWidth() / 2.0 - ImGui::GetStyle().FramePadding.y;
		ImGui::PushItemWidth(width);
		ImGui::SliderFloat("##Morph Z", &morphZ, 0.0, BANK_LEN - 1, "Morph Z: %.3f");
		ImGui::SameLine();
		ImGui::SliderFloat("##Morph Z Speed", &morphZSpeed, 0.f, 10.f, "Morph Z Speed: %.3f Hz", 3.f);
	}

	refreshMorphSnap();
}


void renderToolSelector(Tool *tool) {
	if (ImGui::RadioButton("Pencil", *tool == PENCIL_TOOL)) *tool = PENCIL_TOOL;
	ImGui::SameLine();
	if (ImGui::RadioButton("Brush", *tool == BRUSH_TOOL)) *tool = BRUSH_TOOL;
	ImGui::SameLine();
	if (ImGui::RadioButton("Grab", *tool == GRAB_TOOL)) *tool = GRAB_TOOL;
	ImGui::SameLine();
	if (ImGui::RadioButton("Line", *tool == LINE_TOOL)) *tool = LINE_TOOL;
	ImGui::SameLine();
	if (ImGui::RadioButton("Eraser", *tool == ERASER_TOOL)) *tool = ERASER_TOOL;
	ImGui::SameLine();
	if (ImGui::RadioButton("Smooth", *tool == SMOOTH_TOOL)) *tool = SMOOTH_TOOL;
}


void effectSlider(EffectID effect) {
	char id[64];
	snprintf(id, sizeof(id), "##%s", effectNames[effect]);
	char text[64];
	snprintf(text, sizeof(text), "%s: %%.3f", effectNames[effect]);
	if (ImGui::SliderFloat(id, &currentBank.waves[selectedId].effects[effect], 0.0f, 1.0f, text)) {
		currentBank.waves[selectedId].updatePost();
		historyPush();
	}
}


void editorPage() {
	ImGui::BeginChild("Sidebar", ImVec2(200, 0), true);
	{
		float dummyZ = 0.0;
		ImGui::PushItemWidth(-1);
		renderBankGrid("SidebarGrid", BANK_LEN * 35.0, 1, &dummyZ, &morphZ);
		refreshMorphSnap();
	}
	ImGui::EndChild();

	ImGui::SameLine();
	ImGui::BeginChild("Editor", ImVec2(0, 0), true);
	{
		Wave *wave = &currentBank.waves[selectedId];
		float *effects = wave->effects;

		ImGui::PushItemWidth(-1);

		static enum Tool tool = PENCIL_TOOL;
		renderToolSelector(&tool);

		// ImGui::Text("Waveform");
		ImGui::SameLine();
		if (ImGui::Button("Clear")) {
			currentBank.waves[selectedId].clear();
			historyPush();
		}


		for (const CatalogCategory &catalogCategory : catalogCategories) {
			ImGui::SameLine();
			if (ImGui::Button(catalogCategory.name.c_str())) ImGui::OpenPopup(catalogCategory.name.c_str());
			if (ImGui::BeginPopup(catalogCategory.name.c_str())) {
				for (const CatalogFile &catalogFile : catalogCategory.files) {
					if (ImGui::Selectable(catalogFile.name.c_str())) {
						memcpy(currentBank.waves[selectedId].samples, catalogFile.samples, sizeof(float) * WAVE_LEN);
						currentBank.waves[selectedId].commitSamples();
						historyPush();
					}
				}
				ImGui::EndPopup();
			}
		}

		// ImGui::SameLine();
		// if (ImGui::RadioButton("Smooth", tool == SMOOTH_TOOL)) tool = SMOOTH_TOOL;

		ImGui::Text("Waveform");
		const int oversample = 4;
		float waveOversample[WAVE_LEN * oversample];
		cyclicOversample(wave->postSamples, waveOversample, WAVE_LEN, oversample);
		if (renderWave("WaveEditor", 200.0, wave->samples, WAVE_LEN, waveOversample, WAVE_LEN * oversample, tool)) {
			currentBank.waves[selectedId].commitSamples();
			historyPush();
		}

		ImGui::Text("Harmonics");
		if (renderHistogram("HarmonicEditor", 200.0, wave->harmonics, WAVE_LEN, wave->postHarmonics, WAVE_LEN, tool)) {
			currentBank.waves[selectedId].commitHarmonics();
			historyPush();
		}

		ImGui::Text("Effects");
		for (int i = 0; i < EFFECTS_LEN; i++) {
			effectSlider((EffectID) i);
		}

		if (ImGui::Checkbox("Cycle", &currentBank.waves[selectedId].cycle)) {
			currentBank.waves[selectedId].updatePost();
			historyPush();
		}
		ImGui::SameLine();
		if (ImGui::Checkbox("Normalize", &currentBank.waves[selectedId].normalize)) {
			currentBank.waves[selectedId].updatePost();
			historyPush();
		}
		ImGui::SameLine();
		if (ImGui::Button("Randomize")) {
			currentBank.waves[selectedId].randomizeEffects();
			historyPush();
		}
		ImGui::SameLine();
		if (ImGui::Button("Reset")) {
			currentBank.waves[selectedId].clearEffects();
			historyPush();
		}
		ImGui::SameLine();
		if (ImGui::Button("Bake")) {
			currentBank.waves[selectedId].bakeEffects();
			historyPush();
		}

		ImGui::PopItemWidth();
	}
	ImGui::EndChild();
}


void effectHistogram(EffectID effect, Tool tool) {
	float value[BANK_LEN];
	float average = 0.0;
	for (int i = 0; i < BANK_LEN; i++) {
		value[i] = currentBank.waves[i].effects[effect];
		average += value[i];
	}
	average /= BANK_LEN;
	float oldAverage = average;

	ImGui::Text("%s", effectNames[effect]);

	char id[64];
	snprintf(id, sizeof(id), "##%sAverage", effectNames[effect]);
	char text[64];
	snprintf(text, sizeof(text), "Average %s: %%.3f", effectNames[effect]);
	if (ImGui::SliderFloat(id, &average, 0.0f, 1.0f, text)) {
		// Change the average effect level to the new average
		float deltaAverage = average - oldAverage;
		for (int i = 0; i < BANK_LEN; i++) {
			if (0.0 < average && average < 1.0) {
				currentBank.waves[i].effects[effect] = clampf(currentBank.waves[i].effects[effect] + deltaAverage, 0.0, 1.0);
			}
			else {
				currentBank.waves[i].effects[effect] = average;
			}
			currentBank.waves[i].updatePost();
			historyPush();
		}
	}

	if (renderHistogram(effectNames[effect], 120, value, BANK_LEN, NULL, 0, tool)) {
		for (int i = 0; i < BANK_LEN; i++) {
			if (currentBank.waves[i].effects[effect] != value[i]) {
				// TODO This always selects the highest index. Select the index the mouse is hovering (requires renderHistogram() to return an int)
				selectWave(i);
				currentBank.waves[i].effects[effect] = value[i];
				currentBank.waves[i].updatePost();
				historyPush();
			}
		}
	}
}


void effectPage() {
	ImGui::BeginChild("Effect Editor", ImVec2(0, 0), true); {
		static Tool tool = PENCIL_TOOL;
		renderToolSelector(&tool);

		ImGui::PushItemWidth(-1);
		for (int i = 0; i < EFFECTS_LEN; i++) {
			effectHistogram((EffectID) i, tool);
		}
		ImGui::PopItemWidth();

		if (ImGui::Button("Cycle All")) {
			for (int i = 0; i < BANK_LEN; i++) {
				currentBank.waves[i].cycle = true;
				currentBank.waves[i].updatePost();
				historyPush();
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("Cycle None")) {
			for (int i = 0; i < BANK_LEN; i++) {
				currentBank.waves[i].cycle = false;
				currentBank.waves[i].updatePost();
				historyPush();
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("Normalize All")) {
			for (int i = 0; i < BANK_LEN; i++) {
				currentBank.waves[i].normalize = true;
				currentBank.waves[i].updatePost();
				historyPush();
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("Normalize None")) {
			for (int i = 0; i < BANK_LEN; i++) {
				currentBank.waves[i].normalize = false;
				currentBank.waves[i].updatePost();
				historyPush();
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("Randomize")) {
			for (int i = 0; i < BANK_LEN; i++) {
				currentBank.waves[i].randomizeEffects();
				historyPush();
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("Reset")) {
			for (int i = 0; i < BANK_LEN; i++) {
				currentBank.waves[i].clearEffects();
				historyPush();
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("Bake")) {
			for (int i = 0; i < BANK_LEN; i++) {
				currentBank.waves[i].bakeEffects();
				historyPush();
			}
		}
	}
	ImGui::EndChild();
}


void gridPage() {
	playModeXY = true;
	ImGui::BeginChild("Grid Page", ImVec2(0, 0), true);
	{
		ImGui::PushItemWidth(-1.0);
		renderBankGrid("WaveGrid", -1.f, BANK_GRID_WIDTH, &morphX, &morphY);
		refreshMorphSnap();
	}
	ImGui::EndChild();
}


void waterfallPage() {
	ImGui::BeginChild("3D View", ImVec2(0, 0), true);
	{
		ImGui::PushItemWidth(-1.0);
		static float amplitude = 0.25;
		ImGui::SliderFloat("##amplitude", &amplitude, 0.01, 1.0, "Scale: %.3f", 0.0);
		static float angle = 1.0;
		ImGui::SliderFloat("##angle", &angle, 0.0, 1.0, "Angle: %.3f");

		renderWaterfall("##waterfall", -1.0, amplitude, angle, &morphZ);
	}
	ImGui::EndChild();
}


void renderMain() {
	ImGui::SetNextWindowPos(ImVec2(0, 0));
	ImGui::SetNextWindowSize(ImVec2((int)ImGui::GetIO().DisplaySize.x, (int)ImGui::GetIO().DisplaySize.y));

	ImGui::Begin("", NULL, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_MenuBar);
	{
		// Menu bar
		renderMenu();
		renderPreview();
		// Tab bar
		{
			static const char *tabLabels[NUM_PAGES] = {
				"Waveform Editor",
				"Effect Editor",
				"Grid XY View",
				"Waterfall View",
				"Import",
			};
			static int hoveredTab = 0;
			ImGui::TabLabels(NUM_PAGES, tabLabels, (int*)&currentPage, NULL, false, &hoveredTab);
		}

		// Page
		// Reset some audio variables. These might be changed within the pages.
		playModeXY = false;
		playingBank = &currentBank;
		switch (currentPage) {
		case EDITOR_PAGE: editorPage(); break;
		case EFFECT_PAGE: effectPage(); break;
		case GRID_PAGE: gridPage(); break;
		case WATERFALL_PAGE: waterfallPage(); break;
		case IMPORT_PAGE: importPage(); break;
		default: break;
		}
	}
	ImGui::End();

	if (showTestWindow) {
		ImGui::ShowTestWindow(&showTestWindow);
	}
}


static void refreshStyle() {
	const ImVec4 transparent = ImVec4(0.0, 0.0, 0.0, 0.0);

	ImGuiStyle& style = ImGui::GetStyle();

	style.Alpha = 1.f;
	style.WindowRounding = 1.f;
	style.GrabRounding = 1.f;
	style.ChildWindowRounding = 1.f;
	style.ScrollbarRounding = 1.f;
	style.FrameRounding = 1.f;
	style.FramePadding = ImVec2(4.0f, 4.0f);

	if (styleId == 0) {
		// base16-ashes
		ImVec4 base[5] = {
			ImGui::ColorConvertU32ToFloat4(IM_COL32(0xb9, 0xb9, 0xba, 0xff)), //text
			ImGui::ColorConvertU32ToFloat4(IM_COL32(0xa1, 0xa1, 0xa2, 0xff)), //fg
			ImGui::ColorConvertU32ToFloat4(IM_COL32(0x31, 0x31, 0x32, 0xff)), //bg
			ImGui::ColorConvertU32ToFloat4(IM_COL32(0x11, 0x11, 0x12, 0xff)), //dark bg
			ImGui::ColorConvertU32ToFloat4(IM_COL32(0x61, 0xc1, 0x02, 0xff)), //highlight
		};

		style.Colors[ImGuiCol_Text]                 = base[0x0];
		style.Colors[ImGuiCol_TextDisabled]         = darken(base[0x0], 0.3);
		style.Colors[ImGuiCol_WindowBg]             = lighten(base[0x2], 0.05);
		style.Colors[ImGuiCol_ChildWindowBg]        = base[0x2];
		style.Colors[ImGuiCol_PopupBg]              = base[0x2];
		style.Colors[ImGuiCol_Border]               = alpha(base[0x3], 0.1);
		style.Colors[ImGuiCol_BorderShadow]         = alpha(base[0x3], 0.1);
		style.Colors[ImGuiCol_FrameBg]              = alpha(base[0x3], 0.5);
		style.Colors[ImGuiCol_FrameBgHovered]       = alpha(base[0x3], 0.5);
		style.Colors[ImGuiCol_FrameBgActive]        = base[0x3];
		style.Colors[ImGuiCol_TitleBg]              = base[0x2];
		style.Colors[ImGuiCol_TitleBgCollapsed]     = base[0x2];
		style.Colors[ImGuiCol_TitleBgActive]        = base[0x2];
		style.Colors[ImGuiCol_MenuBarBg]            = lighten(base[0x2], 0.05);
		style.Colors[ImGuiCol_ScrollbarBg]          = transparent;
		style.Colors[ImGuiCol_ScrollbarGrab]        = base[0x1];
		style.Colors[ImGuiCol_ScrollbarGrabHovered] = base[0x1];
		style.Colors[ImGuiCol_ScrollbarGrabActive]  = base[0x1];
		style.Colors[ImGuiCol_ComboBg]              = base[0x3];
		style.Colors[ImGuiCol_CheckMark]            = base[0x1];
		style.Colors[ImGuiCol_SliderGrab]           = base[0x1];
		style.Colors[ImGuiCol_SliderGrabActive]     = base[0x1];
		style.Colors[ImGuiCol_Button]               = alpha(base[0x3], 0.5);
		style.Colors[ImGuiCol_ButtonHovered]        = alpha(base[0x3], 0.5);
		style.Colors[ImGuiCol_ButtonActive]         = alpha(base[0x4], 0.5);
		style.Colors[ImGuiCol_Header]               = lighten(base[0x2], 0.05);
		style.Colors[ImGuiCol_HeaderHovered]        = alpha(base[0x4], 0.5);
		style.Colors[ImGuiCol_HeaderActive]         = alpha(base[0x4], 0.5);
		style.Colors[ImGuiCol_Column]               = lighten(base[0x2], 0.05);
		style.Colors[ImGuiCol_ColumnHovered]        = alpha(base[0x1], 0.1);
		style.Colors[ImGuiCol_ColumnActive]         = alpha(base[0x4], 0.5);
		style.Colors[ImGuiCol_ResizeGrip]           = base[0x2];
		style.Colors[ImGuiCol_ResizeGripHovered]    = base[0x2];
		style.Colors[ImGuiCol_ResizeGripActive]     = base[0x2];
		style.Colors[ImGuiCol_CloseButton]          = base[0x2];
		style.Colors[ImGuiCol_CloseButtonHovered]   = lighten(base[0x2], 0.05);
		style.Colors[ImGuiCol_CloseButtonActive]    = lighten(base[0x2], 0.05);
		style.Colors[ImGuiCol_PlotLines]            = alpha(base[0x1], 0.5);
		style.Colors[ImGuiCol_PlotLinesHovered]     = alpha(base[0x4], 0.5);
		style.Colors[ImGuiCol_PlotHistogram]        = alpha(base[0x4], 0.5);
		style.Colors[ImGuiCol_PlotHistogramHovered] = alpha(base[0x1], 0.5);
		style.Colors[ImGuiCol_TextSelectedBg]       = base[0x3];
		style.Colors[ImGuiCol_ModalWindowDarkening] = alpha(base[0x2], 0.5);
	}
	else if (styleId == 1) {
		// base16-ashes
		ImVec4 base[5] = {
			ImGui::ColorConvertU32ToFloat4(IM_COL32(0x36, 0x36, 0x35, 0xff)), //text
			ImGui::ColorConvertU32ToFloat4(IM_COL32(0x51, 0x51, 0x50, 0xff)), //fg
			ImGui::ColorConvertU32ToFloat4(IM_COL32(0xd8, 0xd8, 0xd7, 0xff)), //bg
			ImGui::ColorConvertU32ToFloat4(IM_COL32(0xae, 0xae, 0xad, 0xff)), //dark bg
			ImGui::ColorConvertU32ToFloat4(IM_COL32(0x61, 0xc1, 0x02, 0xff)), //highlight
		};

		style.Colors[ImGuiCol_Text]                 = base[0x0];
		style.Colors[ImGuiCol_TextDisabled]         = darken(base[0x0], 0.3);
		style.Colors[ImGuiCol_WindowBg]             = lighten(base[0x2], 0.25);
		style.Colors[ImGuiCol_ChildWindowBg]        = base[0x2];
		style.Colors[ImGuiCol_PopupBg]              = base[0x2];
		style.Colors[ImGuiCol_Border]               = alpha(base[0x3], 0.1);
		style.Colors[ImGuiCol_BorderShadow]         = alpha(base[0x3], 0.1);
		style.Colors[ImGuiCol_FrameBg]              = alpha(base[0x3], 0.5);
		style.Colors[ImGuiCol_FrameBgHovered]       = alpha(base[0x3], 0.5);
		style.Colors[ImGuiCol_FrameBgActive]        = base[0x3];
		style.Colors[ImGuiCol_TitleBg]              = base[0x2];
		style.Colors[ImGuiCol_TitleBgCollapsed]     = base[0x2];
		style.Colors[ImGuiCol_TitleBgActive]        = base[0x2];
		style.Colors[ImGuiCol_MenuBarBg]            = lighten(base[0x2], 0.25);
		style.Colors[ImGuiCol_ScrollbarBg]          = transparent;
		style.Colors[ImGuiCol_ScrollbarGrab]        = base[0x1];
		style.Colors[ImGuiCol_ScrollbarGrabHovered] = base[0x1];
		style.Colors[ImGuiCol_ScrollbarGrabActive]  = base[0x1];
		style.Colors[ImGuiCol_ComboBg]              = base[0x3];
		style.Colors[ImGuiCol_CheckMark]            = base[0x1];
		style.Colors[ImGuiCol_SliderGrab]           = base[0x1];
		style.Colors[ImGuiCol_SliderGrabActive]     = base[0x1];
		style.Colors[ImGuiCol_Button]               = alpha(base[0x3], 0.5);
		style.Colors[ImGuiCol_ButtonHovered]        = alpha(base[0x3], 0.5);
		style.Colors[ImGuiCol_ButtonActive]         = alpha(base[0x4], 0.5);
		style.Colors[ImGuiCol_Header]               = lighten(base[0x2], 0.25);
		style.Colors[ImGuiCol_HeaderHovered]        = alpha(base[0x4], 0.5);
		style.Colors[ImGuiCol_HeaderActive]         = alpha(base[0x4], 0.5);
		style.Colors[ImGuiCol_Column]               = lighten(base[0x2], 0.25);
		style.Colors[ImGuiCol_ColumnHovered]        = alpha(base[0x1], 0.1);
		style.Colors[ImGuiCol_ColumnActive]         = alpha(base[0x4], 0.5);
		style.Colors[ImGuiCol_ResizeGrip]           = base[0x2];
		style.Colors[ImGuiCol_ResizeGripHovered]    = base[0x2];
		style.Colors[ImGuiCol_ResizeGripActive]     = base[0x2];
		style.Colors[ImGuiCol_CloseButton]          = base[0x2];
		style.Colors[ImGuiCol_CloseButtonHovered]   = lighten(base[0x2], 0.25);
		style.Colors[ImGuiCol_CloseButtonActive]    = lighten(base[0x2], 0.25);
		style.Colors[ImGuiCol_PlotLines]            = alpha(base[0x1], 0.5);
		style.Colors[ImGuiCol_PlotLinesHovered]     = alpha(base[0x4], 0.5);
		style.Colors[ImGuiCol_PlotHistogram]        = alpha(base[0x4], 0.5);
		style.Colors[ImGuiCol_PlotHistogramHovered] = alpha(base[0x1], 0.5);
		style.Colors[ImGuiCol_TextSelectedBg]       = base[0x3];
		style.Colors[ImGuiCol_ModalWindowDarkening] = alpha(base[0x2], 0.5);
	}
}


void uiInit() {
	ImGui::GetIO().IniFilename = NULL;
	styleId = 0;

	// Load fonts
	ImGui::GetIO().Fonts->AddFontFromFileTTF("fonts/Lekton-Regular.ttf", 15.0);

	// Load UI settings
	// If this gets any more complicated, it should be JSON.
	{
		FILE *f = fopen("ui.dat", "rb");
		if (f) {
			fread(&styleId, sizeof(styleId), 1, f);
			fclose(f);
		}
	}

	refreshStyle();
}


void uiDestroy() {
	// Save UI settings
	{
		FILE *f = fopen("ui.dat", "wb");
		if (f) {
			fwrite(&styleId, sizeof(styleId), 1, f);
			fclose(f);
		}
	}
}


void uiRender() {
	renderMain();
}
