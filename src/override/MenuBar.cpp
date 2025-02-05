/*
 * DISTRHO Cardinal Plugin
 * Copyright (C) 2021 Filipe Coelho <falktx@falktx.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 3 of
 * the License, or any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * For a full copy of the GNU General Public License see the LICENSE file.
 */

/**
 * This file is an edited version of VCVRack's MenuBar.cpp
 * Copyright (C) 2016-2021 VCV.
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 3 of
 * the License, or (at your option) any later version.
 */

#include <thread>
#include <utility>

#include <app/MenuBar.hpp>
#include <app/TipWindow.hpp>
#include <widget/OpaqueWidget.hpp>
#include <ui/Button.hpp>
#include <ui/MenuItem.hpp>
#include <ui/MenuSeparator.hpp>
#include <ui/SequentialLayout.hpp>
#include <ui/Slider.hpp>
#include <ui/TextField.hpp>
#include <ui/PasswordField.hpp>
#include <ui/ProgressBar.hpp>
#include <ui/Label.hpp>
#include <engine/Engine.hpp>
#include <window/Window.hpp>
#include <asset.hpp>
#include <context.hpp>
#include <settings.hpp>
#include <helpers.hpp>
#include <system.hpp>
#include <plugin.hpp>
#include <patch.hpp>
#include <library.hpp>

#ifdef NDEBUG
# undef DEBUG
#endif

// for finding home dir
#ifndef ARCH_WIN
# include <pwd.h>
# include <unistd.h>
#endif

#ifdef HAVE_LIBLO
# include <lo/lo.h>
#endif

#include <Window.hpp>
#include "../AsyncDialog.hpp"
#include "../PluginContext.hpp"

// #define REMOTE_HOST "localhost"
#define REMOTE_HOST "192.168.51.1"
#define REMOTE_HOST_PORT "2228"

namespace rack {
namespace app {
namespace menuBar {


struct MenuButton : ui::Button {
	void step() override {
		box.size.x = bndLabelWidth(APP->window->vg, -1, text.c_str()) + 1.0;
		Widget::step();
	}
	void draw(const DrawArgs& args) override {
		BNDwidgetState state = BND_DEFAULT;
		if (APP->event->hoveredWidget == this)
			state = BND_HOVER;
		if (APP->event->draggedWidget == this)
			state = BND_ACTIVE;
		bndMenuItem(args.vg, 0.0, 0.0, box.size.x, box.size.y, state, -1, text.c_str());
		Widget::draw(args);
	}
};


////////////////////
// File
////////////////////


static void promptClear(const char* const message, const std::function<void()> action)
{
	if (APP->history->isSaved() || APP->scene->rack->hasModules())
		return action();

	asyncDialog::create(message, action);
}

struct FileButton : MenuButton {
	Window& window;
	const bool isStandalone;

#ifdef HAVE_LIBLO
	bool oscConnected = false;
	lo_server oscServer = nullptr;

	static int osc_handler(const char* const path, const char* const types, lo_arg** argv, const int argc, lo_message, void* const self)
	{
		d_stdout("osc_handler(\"%s\", \"%s\", %p, %i)", path, types, argv, argc);

		if (std::strcmp(path, "/resp") == 0 && argc == 2 && types[0] == 's' && types[1] == 's') {
			d_stdout("osc_handler(\"%s\", ...) - got resp | '%s' '%s'", path, &argv[0]->s, &argv[1]->s);
			if (std::strcmp(&argv[0]->s, "hello") == 0 && std::strcmp(&argv[1]->s, "ok") == 0)
				static_cast<FileButton*>(self)->oscConnected = true;
		}
		return 0;
	}

	~FileButton() {
		lo_server_free(oscServer);
	}
#endif

	FileButton(Window& win, const bool standalone)
		: MenuButton(), window(win), isStandalone(standalone) {}

	void onAction(const ActionEvent& e) override {
		ui::Menu* menu = createMenu();
		menu->cornerFlags = BND_CORNER_TOP;
		menu->box.pos = getAbsoluteOffset(math::Vec(0, box.size.y));

		menu->addChild(createMenuItem("New", RACK_MOD_CTRL_NAME "+N", []() {
			// see APP->patch->loadTemplateDialog();
			promptClear("The current patch is unsaved. Clear it and start a new patch?", []() {
				APP->patch->loadTemplate();
			});
		}));

		menu->addChild(createMenuItem("Import...", RACK_MOD_CTRL_NAME "+O", [this]() {
			// see APP->patch->loadDialog();
			promptClear("The current patch is unsaved. Clear it and open a new patch?", [this]() {
				std::string dir;
				if (! APP->patch->path.empty())
				{
					dir = system::getDirectory(APP->patch->path);
				}
				else
				{
					// find home directory
#ifdef ARCH_WIN
					if (const char* const userprofile = getenv("USERPROFILE"))
					{
						dir = userprofile;
					}
					else if (const char* const homedrive = getenv("HOMEDRIVE"))
					{
						if (const char* const homepath = getenv("HOMEPATH"))
							dir = system::join(homedrive, homepath);
					}
#else
					if (struct passwd* const pwd = getpwuid(getuid()))
						dir = pwd->pw_dir;
					else if (const char* const home = getenv("HOME"))
						dir = home;
#endif
				}

				Window::FileBrowserOptions opts;
				opts.startDir = dir.c_str();
				window.openFileBrowser(opts);
			});
		}));

		/*
		menu->addChild(createMenuItem("Save", RACK_MOD_CTRL_NAME "+S", []() {
			APP->patch->saveDialog();
		}));

		menu->addChild(createMenuItem("Save as", RACK_MOD_CTRL_NAME "+Shift+S", []() {
			APP->patch->saveAsDialog();
		}));

		menu->addChild(createMenuItem("Save template", "", []() {
			APP->patch->saveTemplateDialog();
		}));
		*/

#ifdef HAVE_LIBLO
		if (oscServer == nullptr || !oscConnected) {
			menu->addChild(createMenuItem("Connect to MOD", "", [this]() {
				if (oscServer == nullptr) {
					oscServer = lo_server_new_with_proto(nullptr, LO_UDP, nullptr);
					DISTRHO_SAFE_ASSERT_RETURN(oscServer != nullptr,);
					lo_server_add_method(oscServer, "/resp", nullptr, osc_handler, this);
				}
				const lo_address addr = lo_address_new_with_proto(LO_UDP, REMOTE_HOST, REMOTE_HOST_PORT);
				DISTRHO_SAFE_ASSERT_RETURN(addr != nullptr,);
				lo_send(addr, "/hello", "");
				lo_address_free(addr);
			}));
		} else {
			menu->addChild(createMenuItem("Deploy to MOD", "", []() {
				const lo_address addr = lo_address_new_with_proto(LO_UDP, REMOTE_HOST, REMOTE_HOST_PORT);
				DISTRHO_SAFE_ASSERT_RETURN(addr != nullptr,);

				APP->engine->prepareSave();
				APP->patch->saveAutosave();
				APP->patch->cleanAutosave();
				std::vector<uint8_t> data(rack::system::archiveDirectory(APP->patch->autosavePath, 1));

				if (const lo_blob blob = lo_blob_new(data.size(), data.data()))
				{
					lo_send(addr, "/load", "b", blob);
					lo_blob_free(blob);
				}

				lo_address_free(addr);
			}));
		}
#endif

		menu->addChild(createMenuItem("Revert", RACK_MOD_CTRL_NAME "+" RACK_MOD_SHIFT_NAME "+O", []() {
			// APP->patch->revertDialog();
			if (APP->patch->path.empty())
				return;
			promptClear("Revert patch to the last saved state?", []{
				APP->patch->loadAction(APP->patch->path);
			});
		}, APP->patch->path.empty()));

		if (isStandalone) {
			menu->addChild(new ui::MenuSeparator);

			menu->addChild(createMenuItem("Quit", RACK_MOD_CTRL_NAME "+Q", []() {
				APP->window->close();
			}));
		};
	}

#ifdef HAVE_LIBLO
	void step() override {
		MenuButton::step();
		if (oscServer != nullptr) {
			while (lo_server_recv_noblock(oscServer, 0) != 0) {}
		}
	}
#endif
};


////////////////////
// Edit
////////////////////


struct EditButton : MenuButton {
	void onAction(const ActionEvent& e) override {
		ui::Menu* menu = createMenu();
		menu->cornerFlags = BND_CORNER_TOP;
		menu->box.pos = getAbsoluteOffset(math::Vec(0, box.size.y));

		struct UndoItem : ui::MenuItem {
			void step() override {
				text = "Undo " + APP->history->getUndoName();
				disabled = !APP->history->canUndo();
				MenuItem::step();
			}
			void onAction(const ActionEvent& e) override {
				APP->history->undo();
			}
		};
		menu->addChild(createMenuItem<UndoItem>("", RACK_MOD_CTRL_NAME "+Z"));

		struct RedoItem : ui::MenuItem {
			void step() override {
				text = "Redo " + APP->history->getRedoName();
				disabled = !APP->history->canRedo();
				MenuItem::step();
			}
			void onAction(const ActionEvent& e) override {
				APP->history->redo();
			}
		};
		menu->addChild(createMenuItem<RedoItem>("", RACK_MOD_CTRL_NAME "+" RACK_MOD_SHIFT_NAME "+Z"));

		menu->addChild(createMenuItem("Clear cables", "", [=]() {
			APP->patch->disconnectDialog();
		}));

		menu->addChild(new ui::MenuSeparator);

		APP->scene->rack->appendSelectionContextMenu(menu);
	}
};


////////////////////
// View
////////////////////


struct ZoomQuantity : Quantity {
	void setValue(float value) override {
		APP->scene->rackScroll->setZoom(std::pow(2.f, value));
	}
	float getValue() override {
		return std::log2(APP->scene->rackScroll->getZoom());
	}
	float getMinValue() override {
		return -2.f;
	}
	float getMaxValue() override {
		return 2.f;
	}
	float getDefaultValue() override {
		return 0.0;
	}
	float getDisplayValue() override {
		return std::round(std::pow(2.f, getValue()) * 100);
	}
	void setDisplayValue(float displayValue) override {
		setValue(std::log2(displayValue / 100));
	}
	std::string getLabel() override {
		return "Zoom";
	}
	std::string getUnit() override {
		return "%";
	}
};
struct ZoomSlider : ui::Slider {
	ZoomSlider() {
		quantity = new ZoomQuantity;
	}
	~ZoomSlider() {
		delete quantity;
	}
};


struct CableOpacityQuantity : Quantity {
	void setValue(float value) override {
		settings::cableOpacity = math::clamp(value, getMinValue(), getMaxValue());
	}
	float getValue() override {
		return settings::cableOpacity;
	}
	float getDefaultValue() override {
		return 0.5;
	}
	float getDisplayValue() override {
		return getValue() * 100;
	}
	void setDisplayValue(float displayValue) override {
		setValue(displayValue / 100);
	}
	std::string getLabel() override {
		return "Cable opacity";
	}
	std::string getUnit() override {
		return "%";
	}
};
struct CableOpacitySlider : ui::Slider {
	CableOpacitySlider() {
		quantity = new CableOpacityQuantity;
	}
	~CableOpacitySlider() {
		delete quantity;
	}
};


struct CableTensionQuantity : Quantity {
	void setValue(float value) override {
		settings::cableTension = math::clamp(value, getMinValue(), getMaxValue());
	}
	float getValue() override {
		return settings::cableTension;
	}
	float getDefaultValue() override {
		return 0.5;
	}
	std::string getLabel() override {
		return "Cable tension";
	}
	int getDisplayPrecision() override {
		return 2;
	}
};
struct CableTensionSlider : ui::Slider {
	CableTensionSlider() {
		quantity = new CableTensionQuantity;
	}
	~CableTensionSlider() {
		delete quantity;
	}
};


struct RackBrightnessQuantity : Quantity {
	void setValue(float value) override {
		settings::rackBrightness = math::clamp(value, getMinValue(), getMaxValue());
	}
	float getValue() override {
		return settings::rackBrightness;
	}
	float getDefaultValue() override {
		return 1.0;
	}
	float getDisplayValue() override {
		return getValue() * 100;
	}
	void setDisplayValue(float displayValue) override {
		setValue(displayValue / 100);
	}
	std::string getUnit() override {
		return "%";
	}
	std::string getLabel() override {
		return "Room brightness";
	}
	int getDisplayPrecision() override {
		return 3;
	}
};
struct RackBrightnessSlider : ui::Slider {
	RackBrightnessSlider() {
		quantity = new RackBrightnessQuantity;
	}
	~RackBrightnessSlider() {
		delete quantity;
	}
};


struct HaloBrightnessQuantity : Quantity {
	void setValue(float value) override {
		settings::haloBrightness = math::clamp(value, getMinValue(), getMaxValue());
	}
	float getValue() override {
		return settings::haloBrightness;
	}
	float getDefaultValue() override {
		return 0.25;
	}
	float getDisplayValue() override {
		return getValue() * 100;
	}
	void setDisplayValue(float displayValue) override {
		setValue(displayValue / 100);
	}
	std::string getUnit() override {
		return "%";
	}
	std::string getLabel() override {
		return "Light bloom";
	}
	int getDisplayPrecision() override {
		return 3;
	}
};
struct HaloBrightnessSlider : ui::Slider {
	HaloBrightnessSlider() {
		quantity = new HaloBrightnessQuantity;
	}
	~HaloBrightnessSlider() {
		delete quantity;
	}
};


struct KnobScrollSensitivityQuantity : Quantity {
	void setValue(float value) override {
		value = math::clamp(value, getMinValue(), getMaxValue());
		settings::knobScrollSensitivity = std::pow(2.f, value);
	}
	float getValue() override {
		return std::log2(settings::knobScrollSensitivity);
	}
	float getMinValue() override {
		return std::log2(1e-4f);
	}
	float getMaxValue() override {
		return std::log2(1e-2f);
	}
	float getDefaultValue() override {
		return std::log2(1e-3f);
	}
	float getDisplayValue() override {
		return std::pow(2.f, getValue() - getDefaultValue());
	}
	void setDisplayValue(float displayValue) override {
		setValue(std::log2(displayValue) + getDefaultValue());
	}
	std::string getLabel() override {
		return "Scroll wheel knob sensitivity";
	}
	int getDisplayPrecision() override {
		return 2;
	}
};
struct KnobScrollSensitivitySlider : ui::Slider {
	KnobScrollSensitivitySlider() {
		quantity = new KnobScrollSensitivityQuantity;
	}
	~KnobScrollSensitivitySlider() {
		delete quantity;
	}
};


struct ViewButton : MenuButton {
	void onAction(const ActionEvent& e) override {
		ui::Menu* menu = createMenu();
		menu->cornerFlags = BND_CORNER_TOP;
		menu->box.pos = getAbsoluteOffset(math::Vec(0, box.size.y));

		menu->addChild(createBoolPtrMenuItem("Show tooltips", "", &settings::tooltips));

		ZoomSlider* zoomSlider = new ZoomSlider;
		zoomSlider->box.size.x = 250.0;
		menu->addChild(zoomSlider);

		CableOpacitySlider* cableOpacitySlider = new CableOpacitySlider;
		cableOpacitySlider->box.size.x = 250.0;
		menu->addChild(cableOpacitySlider);

		CableTensionSlider* cableTensionSlider = new CableTensionSlider;
		cableTensionSlider->box.size.x = 250.0;
		menu->addChild(cableTensionSlider);

		RackBrightnessSlider* rackBrightnessSlider = new RackBrightnessSlider;
		rackBrightnessSlider->box.size.x = 250.0;
		menu->addChild(rackBrightnessSlider);

		HaloBrightnessSlider* haloBrightnessSlider = new HaloBrightnessSlider;
		haloBrightnessSlider->box.size.x = 250.0;
		menu->addChild(haloBrightnessSlider);

		menu->addChild(new ui::MenuSeparator);

		// menu->addChild(createBoolPtrMenuItem("Hide cursor while dragging", "", &settings::allowCursorLock));

		static const std::vector<std::string> knobModeLabels = {
			"Linear",
			"Scaled linear",
			"Absolute rotary",
			"Relative rotary",
		};
		static const std::vector<int> knobModes = {0, 2, 3};
		menu->addChild(createSubmenuItem("Knob mode", knobModeLabels[settings::knobMode], [=](ui::Menu* menu) {
			for (int knobMode : knobModes) {
				menu->addChild(createCheckMenuItem(knobModeLabels[knobMode], "",
					[=]() {return settings::knobMode == knobMode;},
					[=]() {settings::knobMode = (settings::KnobMode) knobMode;}
				));
			}
		}));

		menu->addChild(createBoolPtrMenuItem("Scroll wheel knob control", "", &settings::knobScroll));

		KnobScrollSensitivitySlider* knobScrollSensitivitySlider = new KnobScrollSensitivitySlider;
		knobScrollSensitivitySlider->box.size.x = 250.0;
		menu->addChild(knobScrollSensitivitySlider);

		menu->addChild(createBoolPtrMenuItem("Lock module positions", "", &settings::lockModules));
	}
};


////////////////////
// Engine
////////////////////


struct EngineButton : MenuButton {
	void onAction(const ActionEvent& e) override {
		ui::Menu* menu = createMenu();
		menu->cornerFlags = BND_CORNER_TOP;
		menu->box.pos = getAbsoluteOffset(math::Vec(0, box.size.y));

		std::string cpuMeterText = "F3";
		if (settings::cpuMeter)
			cpuMeterText += " " CHECKMARK_STRING;
		menu->addChild(createMenuItem("Performance meters", cpuMeterText, [=]() {
			settings::cpuMeter ^= true;
		}));

		menu->addChild(createSubmenuItem("Threads | DO NOT USE", string::f("%d", settings::threadCount), [=](ui::Menu* menu) {
			// BUG This assumes SMT is enabled.
			int cores = system::getLogicalCoreCount() / 2;

			for (int i = 1; i <= 2 * cores; i++) {
				std::string rightText;
				if (i == cores)
					rightText += "(most modules)";
				else if (i == 1)
					rightText += "(lowest CPU usage)";
				menu->addChild(createCheckMenuItem(string::f("%d", i), rightText,
					[=]() {return settings::threadCount == i;},
					[=]() {settings::threadCount = i;}
				));
			}
		}));
	}
};


////////////////////
// Help
////////////////////


struct HelpButton : MenuButton {
	void onAction(const ActionEvent& e) override {
		ui::Menu* menu = createMenu();
		menu->cornerFlags = BND_CORNER_TOP;
		menu->box.pos = getAbsoluteOffset(math::Vec(0, box.size.y));

		menu->addChild(createMenuItem("Tips", "", [=]() {
			APP->scene->addChild(tipWindowCreate());
		}));

		menu->addChild(createMenuItem("VCV User manual", "F1", [=]() {
			system::openBrowser("https://vcvrack.com/manual/");
		}));

		menu->addChild(createMenuItem("Cardinal Project page", "", [=]() {
			system::openBrowser("https://github.com/DISTRHO/Cardinal/");
		}));

		menu->addChild(new ui::MenuSeparator);

		menu->addChild(createMenuLabel(APP_EDITION + " " + APP_EDITION_NAME));

		menu->addChild(createMenuLabel("VCVRack " + APP_VERSION + " Compatible"));
	}
};


////////////////////
// MenuBar
////////////////////


struct MeterLabel : ui::Label {
	int frameIndex = 0;
	double frameDurationTotal = 0.0;
	double frameDurationAvg = 0.0;
	double uiLastTime = 0.0;
	double uiLastThreadTime = 0.0;
	double uiFrac = 0.0;

	void step() override {
		// Compute frame rate
		double frameDuration = APP->window->getLastFrameDuration();
		frameDurationTotal += frameDuration;
		frameIndex++;
		if (frameDurationTotal >= 1.0) {
			frameDurationAvg = frameDurationTotal / frameIndex;
			frameDurationTotal = 0.0;
			frameIndex = 0;
		}

		// Compute UI thread CPU
		// double time = system::getTime();
		// double uiDuration = time - uiLastTime;
		// if (uiDuration >= 1.0) {
		// 	double threadTime = system::getThreadTime();
		// 	uiFrac = (threadTime - uiLastThreadTime) / uiDuration;
		// 	uiLastThreadTime = threadTime;
		// 	uiLastTime = time;
		// }

		double meterAverage = APP->engine->getMeterAverage();
		double meterMax = APP->engine->getMeterMax();
		text = string::f("%.1f fps  %.1f%% avg  %.1f%% max", 1.0 / frameDurationAvg, meterAverage * 100, meterMax * 100);
		Label::step();
	}
};


struct MenuBar : widget::OpaqueWidget {
	// CardinalPluginContext* const context;
	MeterLabel* meterLabel;

	MenuBar(Window& window, const bool isStandalone)
		: widget::OpaqueWidget()
		// : context(ctx)
    {
		const float margin = 5;
		box.size.y = BND_WIDGET_HEIGHT + 2 * margin;

		ui::SequentialLayout* layout = new ui::SequentialLayout;
		layout->margin = math::Vec(margin, margin);
		layout->spacing = math::Vec(0, 0);
		addChild(layout);

		FileButton* fileButton = new FileButton(window, isStandalone);
		fileButton->text = "File";
		layout->addChild(fileButton);

		EditButton* editButton = new EditButton;
		editButton->text = "Edit";
		layout->addChild(editButton);

		ViewButton* viewButton = new ViewButton;
		viewButton->text = "View";
		layout->addChild(viewButton);

		EngineButton* engineButton = new EngineButton;
		engineButton->text = "Engine";
		layout->addChild(engineButton);

		HelpButton* helpButton = new HelpButton;
		helpButton->text = "Help";
		layout->addChild(helpButton);

		// ui::Label* titleLabel = new ui::Label;
		// titleLabel->color.a = 0.5;
		// layout->addChild(titleLabel);

		meterLabel = new MeterLabel;
		meterLabel->box.pos.y = margin;
		meterLabel->box.size.x = 300;
		meterLabel->alignment = ui::Label::RIGHT_ALIGNMENT;
		meterLabel->color.a = 0.5;
		addChild(meterLabel);
	}

	void draw(const DrawArgs& args) override {
		bndMenuBackground(args.vg, 0.0, 0.0, box.size.x, box.size.y, BND_CORNER_ALL);
		bndBevel(args.vg, 0.0, 0.0, box.size.x, box.size.y);

		Widget::draw(args);
	}

	void step() override {
		meterLabel->box.pos.x = box.size.x - meterLabel->box.size.x - 5;
		Widget::step();
	}
};


} // namespace menuBar


widget::Widget* createMenuBar() {
	return new widget::Widget;
}

widget::Widget* createMenuBar(Window& window, const bool isStandalone) {
	menuBar::MenuBar* menuBar = new menuBar::MenuBar(window, isStandalone);
	return menuBar;
}


} // namespace app
} // namespace rack
