#include "TitledListView.h"
#include "PlayfulApp.h"

using namespace Playful;

namespace {
constexpr float cardHeight = 40.0f;
constexpr float cardPitch = 46.0f;

// a little column of dots showing where in a long list we are
void paintScrollDots(juce::Graphics &g, float x, float top, float bottom,
                     int first, int visible, int total) {
    if (total <= visible)
        return;
    int dots = juce::jmin(8, total);
    float spacing = (bottom - top) / float(dots);
    int from = int(std::floor(first * dots / float(total)));
    int to = int(std::ceil((first + visible) * dots / float(total)));
    for (int i = 0; i < dots; i++) {
        g.setColour(i >= from && i < to ? Colours::muted : Colours::dim);
        fillEllipseCentred(g, x, top + spacing * (i + 0.5f), 3.0f, 3.0f);
    }
}
} // namespace

TitledListView::TitledListView(const juce::StringArray &listItems,
                               const juce::String &titleString,
                               ListTitle::IconType, const juce::String &)
    : listView(listItems), title(titleString) {
    // the list box only keeps the selection; everything is painted here
    addChildComponent(listView);
}

void TitledListView::resized() {
    listView.setBounds(getLocalBounds());
    listView.getListBox().setRowHeight(juce::jmax(1, getHeight() / 6));
}

void TitledListView::setTitleString(juce::String newTitle) {
    title = newTitle;
    repaint();
}

SimpleListView &TitledListView::getListView() { return listView; }

void TitledListView::setListItems(const juce::StringArray &listItems) {
    listView.setListItems(listItems);
    repaint();
}

void TitledListView::setMascot(Mascot m, std::function<juce::String()> caption,
                               std::function<juce::String()> subCaption) {
    mascot = m;
    mascotCaption = std::move(caption);
    mascotSubCaption = std::move(subCaption);
}

int TitledListView::getNumItems() { return listView.getListItems().size(); }

int TitledListView::getSelectedRow() {
    int n = getNumItems();
    if (n == 0)
        return -1;
    return juce::jlimit(0, n - 1, listView.getListBox().getSelectedRow());
}

juce::String TitledListView::getSelectedItemName() {
    int row = getSelectedRow();
    return row >= 0 ? nameFor(row, infoFor(row)) : juce::String();
}

juce::String TitledListView::nameFor(int index, const ListItemInfo &info) {
    return info.name.isNotEmpty() ? info.name : listView.getListItems()[index];
}

ListItemInfo TitledListView::infoFor(int index) {
    return itemInfo ? itemInfo(index) : ListItemInfo();
}

std::array<KnobInfo, 4> TitledListView::getKnobs() {
    if (knobs)
        return knobs();

    std::array<KnobInfo, 4> k;
    int n = getNumItems();
    int row = getSelectedRow();
    k[0] = {primaryLabel, getSelectedItemName(),
            n > 1 ? float(row) / float(n - 1) : 1.0f, Colours::blue, n > 0};
    for (int i = 1; i < 4; i++)
        k[size_t(i)].enabled = false;
    return k;
}

juce::String TitledListView::getChangeSignature() {
    juce::String s = PlayfulScreen::getChangeSignature();
    s << "|" << title << "|" << getSelectedRow() << "|"
      << listView.getListItems().joinIntoString(",");
    for (int i = 0; i < getNumItems(); i++) {
        auto info = infoFor(i);
        s << "|" << info.detail << info.target << int(info.off);
    }
    return s;
}

//==============================================================================
void TitledListView::paintStage(juce::Graphics &g, double time) {
    switch (layout) {
    case Layout::linked:
        paintLinked(g, time);
        break;
    case Layout::chain:
        paintChain(g, time);
        break;
    case Layout::grid:
        paintGrid(g, time);
        break;
    case Layout::list:
    default: {
        float top = 14.0f;
        if (step >= 0) {
            paintSteps(g);
            top = 56.0f;
        }
        bool hasMascot = mascot != Mascot::none;
        paintList(g, time, top, hasMascot ? 520.0f : 744.0f);
        if (hasMascot)
            paintMascot(g, time, 668.0f, step >= 0 ? 170.0f : 140.0f);
        break;
    }
    }
}

void TitledListView::paintList(juce::Graphics &g, double time, float top,
                               float width) {
    int n = getNumItems();
    if (n == 0) {
        drawText(g, emptyMessage, 24, top, width, 260, 20, Colours::muted,
                 true);
        return;
    }

    int visible = int((stageHeight - top - 6.0f) / cardPitch);
    int row = getSelectedRow();
    int first = juce::jlimit(0, juce::jmax(0, n - visible), row - visible / 2);
    for (int i = first; i < juce::jmin(n, first + visible); i++) {
        auto info = infoFor(i);
        float y = top + (i - first) * cardPitch;
        drawCard(g, {24.0f, y, width, cardHeight}, nameFor(i, info),
                 info.detail, Colours::candy(i), i == row, info.off, time);
    }
    paintScrollDots(g, 24.0f + width + 14.0f, top + 6.0f,
                    top + visible * cardPitch - 12.0f, first, visible, n);
}

void TitledListView::paintLinked(juce::Graphics &g, double time) {
    int n = getNumItems();
    if (n == 0) {
        drawText(g, emptyMessage, 24, 14, 752, 230, 20, Colours::muted, true);
        paintMascot(g, time, 400.0f, 250.0f);
        return;
    }

    const float leftWidth = 300.0f, targetX = 560.0f, targetWidth = 216.0f;
    int visible = 6;
    int row = getSelectedRow();
    int first = juce::jlimit(0, juce::jmax(0, n - visible), row - visible / 2);
    for (int i = first; i < juce::jmin(n, first + visible); i++) {
        auto info = infoFor(i);
        float y = 14.0f + (i - first) * cardPitch;
        auto colour = Colours::candy(i);
        drawCard(g, {24.0f, y, leftWidth, cardHeight}, nameFor(i, info),
                 info.detail, colour, i == row, info.off, time);

        if (info.target.isEmpty())
            continue;

        // a dashed wire to what it modulates
        float cy = y + cardHeight / 2.0f;
        juce::Path wire;
        wire.startNewSubPath(24.0f + leftWidth + 8.0f, cy);
        wire.cubicTo(450.0f, cy - 10.0f, 470.0f, cy + 10.0f, targetX - 8.0f,
                     cy);
        drawDashedPath(g, wire, info.off ? Colours::dim : colour, 2.5f, 3.0f,
                       7.0f);

        g.setColour(Colours::panel);
        g.fillRoundedRectangle(targetX, y, targetWidth, cardHeight, 14.0f);
        g.setColour(info.off ? Colours::dim : colour);
        g.drawEllipse(targetX + 10.0f, cy - 9.0f, 18.0f, 18.0f, 3.0f);
        fillEllipseCentred(g, targetX + 19.0f, cy, 4.0f, 4.0f);
        drawText(g, info.target, targetX + 36.0f, y + 4.0f, targetWidth - 44.0f,
                 18.0f, 13.0f, Colours::text, true,
                 juce::Justification::centredLeft);
        drawText(g, info.targetDetail, targetX + 36.0f, y + 20.0f,
                 targetWidth - 44.0f, 15.0f, 11.0f, Colours::muted, false,
                 juce::Justification::centredLeft);
    }
    paintScrollDots(g, 340.0f, 20.0f, 14.0f + visible * cardPitch - 12.0f,
                    first, visible, n);
    if (n <= 3)
        paintMascot(g, time, 668.0f, 240.0f);
}

void TitledListView::paintChain(juce::Graphics &g, double time) {
    int n = getNumItems();
    drawRobot(g, 66.0f, 150.0f, 0.75f, time);

    if (n == 0) {
        drawText(g, emptyMessage, 130, 90, 640, 120, 20, Colours::muted, true);
        return;
    }

    const float x0 = 140.0f, pitch = 128.0f, w = 112.0f, h = 80.0f, y = 110.0f;
    const int slots = 5;
    int row = getSelectedRow();
    int total = n + 1; // the track's own output at the end
    int first = juce::jlimit(0, juce::jmax(0, total - slots), row - 2);

    // the conveyor the sound travels along
    drawDashedLine(g, {110.0f, y + h / 2.0f, 780.0f, y + h / 2.0f},
                   Colours::dim, 3.0f, 8.0f, 6.0f);
    for (int k = 0; k < 22; k++) {
        float x = 110.0f + k * 31.0f +
                  float(loop(time, 1.2)) * (animationsEnabled() ? 31.0f : 0.0f);
        g.setColour(Colours::dim);
        fillEllipseCentred(g, x, y + h + 24.0f, 3.0f, 3.0f);
    }

    for (int i = first; i < juce::jmin(total, first + slots); i++) {
        float x = x0 + (i - first) * pitch;
        bool isOutput = i == n;
        auto info = isOutput ? ListItemInfo{"", "TRACK"} : infoFor(i);
        bool selected = i == row;
        auto colour = isOutput ? Colours::green : Colours::candy(i);
        if (info.off)
            colour = Colours::dim;

        drawText(g, info.kind, x, y - 22.0f, w, 16.0f, 11.0f,
                 selected ? Colours::text : Colours::muted, selected);
        g.setColour(selected ? Colours::dim : Colours::panel);
        g.fillRoundedRectangle(x, y, w, h, 18.0f);
        if (selected) {
            g.setColour(Colours::cream);
            g.drawRoundedRectangle(x, y, w, h, 18.0f, 3.0f);
        }
        drawBadge(g, x + w / 2.0f, y + 28.0f, 16.0f, colour, time);
        auto name = isOutput ? juce::String("Volume") : nameFor(i, info);
        drawText(g, name, x + 4.0f, y + 50.0f, w - 8.0f, 20.0f, 13.0f,
                 info.off ? Colours::muted : Colours::text, true);
        if (info.off)
            drawPill(g, {x + w / 2.0f - 26.0f, y + h - 8.0f, 52.0f, 16.0f},
                     "bypass", Colours::red, Colours::ink, 10.0f);
    }
    if (first > 0)
        drawText(g, utf8("‹"), 116, y + 20, 20, 40, 28, Colours::muted, true);
    if (first + slots < total)
        drawText(g, utf8("›"), 772, y + 20, 20, 40, 28, Colours::muted, true);

    drawText(g,
             utf8("sound travels left to right  ·  + add  ·  − remove  ·  "
                  "press 4: bypass"),
             24, 262, 752, 20, 13, Colours::muted);
}

void TitledListView::paintGrid(juce::Graphics &g, double time) {
    int n = getNumItems();
    const int cols = 3, rowsVisible = 2;
    const float w = 236.0f, h = 140.0f;
    int row = getSelectedRow();
    int total = n + 1; // a "new" card at the end
    int totalRows = (total + cols - 1) / cols;
    int selectedGridRow = juce::jmax(0, row) / cols;
    int firstRow = juce::jlimit(0, juce::jmax(0, totalRows - rowsVisible),
                                selectedGridRow - 1);
    const auto &items = listView.getListItems();

    for (int i = firstRow * cols;
         i < juce::jmin(total, (firstRow + rowsVisible) * cols); i++) {
        int r = i / cols - firstRow, c = i % cols;
        float x = 24.0f + c * (w + 22.0f), y = 14.0f + r * (h + 14.0f);
        bool isNew = i == n;
        bool selected = i == row;

        g.setColour(selected ? Colours::dim : Colours::panel);
        g.fillRoundedRectangle(x, y, w, h, 18.0f);
        if (selected) {
            g.setColour(Colours::cream);
            g.drawRoundedRectangle(x, y, w, h, 18.0f, 3.0f);
        }

        if (isNew) {
            drawText(g, "+ New project", x + 16.0f, y + 88.0f, w - 32.0f, 20,
                     15, Colours::text, true, juce::Justification::centredLeft);
            drawText(g, "press + anytime", x + 16.0f, y + 108.0f, w - 32.0f, 16,
                     11, Colours::muted, false,
                     juce::Justification::centredLeft);
            continue;
        }

        auto info = infoFor(i);
        // a made-up "song" thumbnail: a few candy bars, the same every time
        // for the same name
        auto hash = items[i].hashCode();
        juce::Random random(hash);
        for (int k = 0; k < 5; k++) {
            float bx = x + 16.0f + random.nextFloat() * 50.0f;
            float bw = 60.0f + random.nextFloat() * (w - 110.0f);
            g.setColour(Colours::candy(int(random.nextInt(6))));
            g.fillRoundedRectangle(bx, y + 16.0f + k * 12.0f, bw, 7.0f, 3.5f);
        }
        if (info.marked) {
            // a little house: this is the open project
            float hx = x + w - 34.0f, hy = y + 16.0f;
            juce::Path roof;
            roof.addTriangle(hx - 4, hy + 10, hx + 10, hy - 2, hx + 24,
                             hy + 10);
            g.setColour(Colours::yellow);
            g.fillPath(roof);
            g.fillRect(hx, hy + 9, 20.0f, 13.0f);
            g.setColour(Colours::ink);
            g.fillRect(hx + 7, hy + 14, 6.0f, 8.0f);
        }
        drawText(g, nameFor(i, info), x + 16.0f, y + 88.0f, w - 32.0f, 20, 15,
                 Colours::text, true, juce::Justification::centredLeft);
        drawText(g, info.detail, x + 16.0f, y + 108.0f, w - 32.0f, 16, 11,
                 Colours::muted, false, juce::Justification::centredLeft);
    }
    juce::ignoreUnused(time);
}

void TitledListView::paintSteps(juce::Graphics &g) {
    static const char *names[] = {"1 · modifier", "2 · plugin", "3 · knob"};
    const float w = 150.0f;
    for (int i = 0; i < 3; i++) {
        float x = 24.0f + i * 276.0f;
        auto fill = i < step    ? Colours::green
                    : i == step ? Colours::yellow
                                : Colours::panel;
        drawPill(g, {x, 10.0f, w, 24.0f}, utf8(names[i]), fill,
                 i <= step ? Colours::ink : Colours::muted, 12.0f);
        if (i < 2)
            drawDashedLine(g, {x + w + 8.0f, 22.0f, x + 268.0f, 22.0f},
                           i < step ? Colours::green : Colours::dim, 2.5f, 3.0f,
                           6.0f);
    }
}

void TitledListView::paintMascot(juce::Graphics &g, double time, float cx,
                                 float cy) {
    paintListMascot(g, mascotChooser ? mascotChooser() : mascot, cx, cy, time,
                    Colours::candy(juce::jmax(0, getSelectedRow())));
    if (mascotCaption)
        drawText(g, mascotCaption(), cx - 110.0f, cy + 74.0f, 220.0f, 24.0f,
                 17.0f, Colours::text, true);
    if (mascotSubCaption)
        drawText(g, mascotSubCaption(), cx - 110.0f, cy + 98.0f, 220.0f, 18.0f,
                 12.0f, Colours::muted);
}

void paintListMascot(juce::Graphics &g, TitledListView::Mascot mascot, float cx,
                     float cy, double time, juce::Colour colour) {
    using Mascot = TitledListView::Mascot;
    switch (mascot) {
    case Mascot::cat:
        drawCat(g, cx, cy, 0.9f, time);
        break;
    case Mascot::bee:
        drawBee(g, cx, cy, 3.6f, time);
        break;
    case Mascot::blob:
        drawBlob(g, cx, cy, 0.8f, Colours::green, time);
        break;
    case Mascot::ball:
        drawBallFriend(g, cx, cy, 46.0f, colour, time);
        break;
    case Mascot::robot:
        drawRobot(g, cx, cy, 0.9f, time);
        break;
    case Mascot::drum:
        drawDrum(g, cx, cy, 0.95f, time);
        break;
    case Mascot::caterpillar:
        drawCaterpillar(g, cx, cy, 1.2f, time);
        break;
    case Mascot::tombola:
        drawTombola(g, cx, cy, 1.1f, time);
        break;
    case Mascot::train:
        drawTrain(g, cx, cy, 1.1f, time);
        break;
    case Mascot::none:
    default:
        break;
    }
}
