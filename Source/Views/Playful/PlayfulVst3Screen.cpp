#include "PlayfulVst3Screen.h"
#include "PlayfulApp.h"

using namespace Playful;
using ExternalVM = app_view_models::ExternalPluginViewModel;

namespace {
std::vector<PlayfulParam> paramsFor(ExternalVM &vm) {
    std::vector<PlayfulParam> params;
    for (auto &page : vm.getPages()) {
        for (auto &knob : page.knobs) {
            if (knob.parameter == nullptr) {
                params.push_back(PlayfulParam::none());
                continue;
            }
            auto parameter = knob.parameter;
            PlayfulParam p;
            p.label = knob.label;
            p.normalised = [parameter] {
                return ExternalVM::getValue(*parameter);
            };
            p.text = [parameter] { return ExternalVM::getText(*parameter); };
            p.nudge = [parameter](int direction) {
                ExternalVM::nudge(*parameter, direction);
            };
            params.push_back(p);
        }
    }
    return params;
}

juce::String bankName(int bank, int perBank, int total) {
    int first = bank * perBank + 1;
    int last = juce::jmin(total, first + perBank - 1);
    return juce::String(first) + utf8("–") + juce::String(last);
}

void drawBook(juce::Graphics &g, float cx, float cy) {
    juce::Path book;
    book.startNewSubPath(cx - 44, cy - 6);
    book.lineTo(cx, cy + 2);
    book.lineTo(cx + 44, cy - 6);
    book.lineTo(cx + 44, cy + 44);
    book.lineTo(cx, cy + 52);
    book.lineTo(cx - 44, cy + 44);
    book.closeSubPath();
    g.setColour(Colours::cream);
    g.fillPath(book);
    g.setColour(Colours::ink);
    g.drawLine(cx, cy + 2, cx, cy + 52, 2.0f);
    g.setColour(Colours::purple);
    for (int i = 0; i < 3; i++) {
        float y = cy + 10.0f + i * 10.0f;
        g.drawLine(cx - 34, y, cx - 10, y + 3, 3.0f);
        g.drawLine(cx + 10, y + 3, cx + 34, y, 3.0f);
    }
}
} // namespace

PlayfulVst3Screen::PlayfulVst3Screen(
    app_services::MidiCommandManager &mcm,
    std::shared_ptr<app_view_models::ExternalPluginViewModel> vm)
    : PlayfulParamScreen(mcm, vm->getPlugin().getName(), paramsFor(*vm),
                         nullptr, vm),
      mcm(mcm), viewModel(vm) {}

//==============================================================================
void PlayfulVst3Screen::openBrowser() {
    if (viewModel->getNumPresets() <= 0)
        return;
    browsing = true;
    browseIndex = juce::jlimit(0, viewModel->getNumPresets() - 1,
                               viewModel->getCurrentPreset());
    repaint();
}

void PlayfulVst3Screen::closeBrowser() {
    browsing = false;
    repaint();
}

void PlayfulVst3Screen::moveBrowse(int delta) {
    browseIndex = juce::jlimit(0, juce::jmax(0, viewModel->getNumPresets() - 1),
                               browseIndex + delta);
    repaint();
}

void PlayfulVst3Screen::encoder1Increased() {
    if (browsing)
        moveBrowse(1);
    else
        PlayfulParamScreen::encoder1Increased();
}

void PlayfulVst3Screen::encoder1Decreased() {
    if (browsing)
        moveBrowse(-1);
    else
        PlayfulParamScreen::encoder1Decreased();
}

void PlayfulVst3Screen::encoder2Increased() {
    if (browsing)
        moveBrowse(presetsPerBank);
    else
        PlayfulParamScreen::encoder2Increased();
}

void PlayfulVst3Screen::encoder2Decreased() {
    if (browsing)
        moveBrowse(-presetsPerBank);
    else
        PlayfulParamScreen::encoder2Decreased();
}

void PlayfulVst3Screen::encoder3Increased() {
    if (!browsing)
        PlayfulParamScreen::encoder3Increased();
}

void PlayfulVst3Screen::encoder3Decreased() {
    if (!browsing)
        PlayfulParamScreen::encoder3Decreased();
}

void PlayfulVst3Screen::encoder4Increased() {
    if (!browsing)
        PlayfulParamScreen::encoder4Increased();
}

void PlayfulVst3Screen::encoder4Decreased() {
    if (!browsing)
        PlayfulParamScreen::encoder4Decreased();
}

void PlayfulVst3Screen::encoder1ButtonReleased() {
    if (!isShowing())
        return;
    if (browsing) {
        viewModel->loadPreset(browseIndex);
        closeBrowser();
    } else {
        openBrowser();
    }
}

void PlayfulVst3Screen::encoder2ButtonReleased() {
    if (browsing && isShowing())
        closeBrowser();
}

void PlayfulVst3Screen::stopButtonReleased() {
    if (browsing && isShowing())
        closeBrowser();
}

void PlayfulVst3Screen::plusButtonReleased() {
    if (!browsing)
        PlayfulParamScreen::plusButtonReleased();
}

void PlayfulVst3Screen::minusButtonReleased() {
    if (!browsing)
        PlayfulParamScreen::minusButtonReleased();
}

//==============================================================================
juce::String PlayfulVst3Screen::getTitle() {
    return viewModel->getPlugin().getName();
}

juce::String PlayfulVst3Screen::getSubtitle() {
    if (browsing)
        return "browsing" + utf8("  ·  ") +
               juce::String(viewModel->getNumPresets()) + " presets";
    const auto &pages = viewModel->getPages();
    auto pageName = juce::isPositiveAndBelow(getPage(), int(pages.size()))
                        ? pages[size_t(getPage())].name
                        : juce::String();
    return "VST3" + utf8("  ·  ") + pageName.toLowerCase();
}

juce::String PlayfulVst3Screen::getPageLabel() {
    if (browsing)
        return "press 1 to load";
    return PlayfulParamScreen::getPageLabel();
}

std::array<KnobInfo, 4> PlayfulVst3Screen::getKnobs() {
    if (!browsing)
        return PlayfulParamScreen::getKnobs();

    std::array<KnobInfo, 4> k;
    int n = viewModel->getNumPresets();
    int banks = (n + presetsPerBank - 1) / presetsPerBank;
    int bank = browseIndex / presetsPerBank;
    k[0] = {utf8("PRESET · LOAD"), viewModel->getPresetName(browseIndex),
            n > 1 ? browseIndex / float(n - 1) : 1.0f, Colours::blue};
    k[1] = {utf8("BANK · BACK"), bankName(bank, presetsPerBank, n),
            banks > 1 ? bank / float(banks - 1) : 1.0f, Colours::green};
    k[2].enabled = false;
    k[3].enabled = false;
    return k;
}

juce::String PlayfulVst3Screen::getChangeSignature() {
    return PlayfulParamScreen::getChangeSignature() + "|" +
           juce::String(int(browsing)) + juce::String(browseIndex) + "|" +
           juce::String(viewModel->getCurrentPreset()) +
           juce::String(int(mcm.isControlDown));
}

void PlayfulVst3Screen::paintStage(juce::Graphics &g, double time) {
    const auto &pages = viewModel->getPages();
    int n = viewModel->getNumPresets();

    if (browsing) {
        // banks of 16 on the left
        int banks = (n + presetsPerBank - 1) / presetsPerBank;
        int bank = browseIndex / presetsPerBank;
        int firstBank = juce::jlimit(0, juce::jmax(0, banks - 6), bank - 2);
        for (int b = firstBank; b < juce::jmin(banks, firstBank + 6); b++) {
            float y = 14.0f + (b - firstBank) * 46.0f;
            bool selected = b == bank;
            g.setColour(selected ? Colours::yellow : Colours::panel);
            g.fillRoundedRectangle(24.0f, y, 150.0f, 40.0f, 14.0f);
            drawText(g, bankName(b, presetsPerBank, n), 40.0f, y, 120.0f, 40.0f,
                     15.0f, selected ? Colours::ink : Colours::text, true,
                     juce::Justification::centredLeft);
        }

        // presets on the right
        int first = juce::jlimit(0, juce::jmax(0, n - 6), browseIndex - 2);
        int current = viewModel->getCurrentPreset();
        for (int i = first; i < juce::jmin(n, first + 6); i++) {
            float y = 14.0f + (i - first) * 46.0f;
            drawCard(g, {194.0f, y, 330.0f, 40.0f},
                     juce::String(i + 1) + "  " + viewModel->getPresetName(i),
                     i == current ? utf8("♪ on") : juce::String(),
                     Colours::candy(i), i == browseIndex, false, time);
        }

        drawRobot(g, 668.0f, 150.0f, 1.0f, time);
        drawBook(g, 668.0f, 172.0f);
        drawText(g, utf8("turn 1: preset  ·  turn 2: bank"), 560, 260, 220, 18,
                 12, Colours::muted);
        return;
    }

    // page tabs along the top, scrolled so the current one shows
    int page = getPage();
    auto tabWidth = [&](int i) {
        return 18.0f + pages[size_t(i)].name.length() * 8.0f;
    };
    int firstTab = 0;
    auto widthFrom = [&](int from) {
        float w = 0.0f;
        for (int i = from; i <= page && i < int(pages.size()); i++)
            w += tabWidth(i) + 8.0f;
        return w;
    };
    while (firstTab < page && widthFrom(firstTab) > 700.0f)
        firstTab++;
    float x = firstTab > 0 ? 40.0f : 24.0f;
    if (firstTab > 0)
        drawText(g, utf8("‹"), 18, 4, 16, 26, 20, Colours::muted, true);
    for (int i = firstTab; i < int(pages.size()); i++) {
        float w = tabWidth(i);
        if (x + w > 760.0f) {
            drawText(g, utf8("›"), 764, 4, 16, 26, 20, Colours::muted, true);
            break;
        }
        drawPill(g, {x, 6.0f, w, 24.0f}, pages[size_t(i)].name,
                 i == page ? Colours::yellow : Colours::panel,
                 i == page ? Colours::ink : Colours::muted, 12.0f);
        x += w + 8.0f;
    }

    drawRobot(g, 116.0f, 170.0f, 1.25f, time);

    // the 8 values of this page: encoders on top, Control row below
    int controlRow = mcm.isControlDown ? 1 : 0;
    if (juce::isPositiveAndBelow(page, int(pages.size()))) {
        const auto &knobs = pages[size_t(page)].knobs;
        for (int slot = 0; slot < 8; slot++) {
            int row = slot / 4, col = slot % 4;
            float cx = 236.0f + col * 136.0f, cy = 44.0f + row * 92.0f;
            juce::Rectangle<float> cell(cx, cy, 128.0f, 84.0f);
            auto &knob = knobs[size_t(slot)];
            if (knob.parameter == nullptr) {
                drawDashedPath(
                    g,
                    [&] {
                        juce::Path p;
                        p.addRoundedRectangle(cell.reduced(2.0f), 16.0f);
                        return p;
                    }(),
                    Colours::dim, 2.0f, 4.0f, 6.0f);
                continue;
            }
            bool active = row == controlRow;
            g.setColour(active ? Colours::dim : Colours::panel);
            g.fillRoundedRectangle(cell, 16.0f);
            auto colour = Colours::encoder(col);
            drawText(g, knob.label, cx + 12.0f, cy + 8.0f, 108.0f, 16.0f, 11.0f,
                     Colours::muted, false, juce::Justification::centredLeft);
            float value = ExternalVM::getValue(*knob.parameter);
            g.setColour(Colours::background);
            g.fillRoundedRectangle(cx + 12.0f, cy + 30.0f, 104.0f, 12.0f, 6.0f);
            g.setColour(active ? colour : colour.withAlpha(0.55f));
            g.fillRoundedRectangle(cx + 12.0f, cy + 30.0f,
                                   juce::jmax(12.0f, 104.0f * value), 12.0f,
                                   6.0f);
            drawText(g, ExternalVM::getText(*knob.parameter), cx + 12.0f,
                     cy + 50.0f, 108.0f, 22.0f, 14.0f, Colours::text, true,
                     juce::Justification::centredLeft);
        }
    }

    // the preset
    g.setColour(Colours::panel);
    g.fillRoundedRectangle(236.0f, 236.0f, 540.0f, 76.0f, 20.0f);
    drawText(g, "PRESET", 256.0f, 246.0f, 200.0f, 16.0f, 11.0f, Colours::muted,
             false, juce::Justification::centredLeft);
    if (n > 0) {
        drawText(g, viewModel->getPresetName(viewModel->getCurrentPreset()),
                 256.0f, 264.0f, 330.0f, 30.0f, 20.0f, Colours::text, true,
                 juce::Justification::centredLeft);
        drawPill(g, {596.0f, 258.0f, 164.0f, 32.0f}, "press 1: presets",
                 Colours::background, Colours::cream, 12.0f);
    } else {
        drawText(g, "this plugin has no presets", 256.0f, 264.0f, 400.0f, 30.0f,
                 16.0f, Colours::muted, false,
                 juce::Justification::centredLeft);
    }
    if (!viewModel->hasMap())
        drawText(g, "no knob map: every parameter, 8 to a page", 24, 300, 200,
                 20, 10, Colours::muted, false,
                 juce::Justification::centredLeft);
}

//==============================================================================
std::unique_ptr<juce::Component>
createPlayfulVst3View(tracktion::Plugin *plugin,
                      app_services::MidiCommandManager &mcm) {
    if (dynamic_cast<tracktion::ExternalPlugin *>(plugin) == nullptr)
        return nullptr;
    namespace KnobMaps = app_view_models::KnobMaps;
    auto directory = KnobMaps::getDirectory();
    KnobMaps::installDefaults(directory);
    auto map = KnobMaps::findMapFor(plugin->getName(), directory);
    auto vm = std::make_shared<ExternalVM>(plugin, map);
    return std::make_unique<PlayfulVst3Screen>(mcm, vm);
}
