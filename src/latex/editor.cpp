#include "editor.h"

#include "microtex/lib/core/formula.h"
#include "microtex/lib/core/parser.h"
#include <algorithm> 
#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui_internal.h"

#define MAX(X,Y) ((X > Y) ? X : Y)
#define MIN(X,Y) ((X < Y) ? X : Y)

bool is_alpha(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}
bool is_digit(char c) {
    return c >= '0' && c <= '9';
}
bool is_alphanum(char c) {
    return is_alpha(c) || is_digit(c);
}
bool is_whitespace(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}
bool is_special_char(char c) {
    return !is_alphanum(c) && !is_whitespace(c);
}

size_t LatexEditor::find_line_begin(size_t pos) {
    while (pos > 0 && m_text[pos - 1] != '\n')
        pos--;
    return pos;
}
size_t LatexEditor::find_line_end(size_t pos) {
    while (pos < m_text.size() && m_text[pos] != '\n')
        pos++;
    return pos;
}
size_t LatexEditor::find_line_number(size_t pos) {
    // Quick for tiny text
    size_t line_number = 0;
    if (m_line_positions.size() <= 1)
        return 0;
    for (auto it = std::next(m_line_positions.begin());it != m_line_positions.end();it++) {
        if (*it > pos)
            break;
        line_number++;
    }
    return line_number;
}
bool LatexEditor::is_line_begin(size_t pos) {
    return m_line_positions.find(pos) != m_line_positions.end();
}

/* ===============================
 *             Events
 * =============================== */
void LatexEditor::set_last_key_pressed(int key) {
    if (m_last_key_pressed == key) {
        m_last_key_pressed_counter++;
    }
    else
        m_last_key_pressed_counter = 0;
    m_last_key_pressed = key;
}
void LatexEditor::events(const Rect& boundaries) {
    auto& io = ImGui::GetIO();
    bool ctrl_or_cmd = io.KeyCtrl;
#ifdef __APPLE__
    ctrl_or_cmd = io.KeySuper;
#endif

    int current_key_presses = ImGui::IsKeyPressed(ImGuiKey_LeftArrow, true) ? K_LEFT : 0
        | ImGui::IsKeyPressed(ImGuiKey_RightArrow, true) ? K_RIGHT : 0
        | ImGui::IsKeyPressed(ImGuiKey_UpArrow, true) ? K_UP : 0
        | ImGui::IsKeyPressed(ImGuiKey_DownArrow, true) ? K_DOWN : 0
        | ImGui::IsKeyPressed(ImGuiKey_Home, true) ? K_HOME : 0
        | ImGui::IsKeyPressed(ImGuiKey_End, true) ? K_END : 0
        | ImGui::IsKeyPressed(ImGuiKey_Delete, true) ? K_DEL : 0
        | ImGui::IsKeyPressed(ImGuiKey_Backspace, true) ? K_BACKSPACE : 0
        | ImGui::IsKeyPressed(ImGuiKey_Tab, true) ? K_TAB : 0
        | ImGui::IsKeyPressed(ImGuiKey_Enter, true) || ImGui::IsKeyPressed(ImGuiKey_KeyPadEnter) ? K_ENTER : 0
        | ImGui::IsKeyPressed(ImGuiKey_Escape, true) ? K_ESCAPE : 0;
    int new_key_presses = current_key_presses & ~m_key_presses;
    int to_press = 0;
    if (m_is_focused) {
        // Keyboard events
        if (new_key_presses != 0) {
            to_press = new_key_presses;
        }
        else if (current_key_presses) {
            to_press = current_key_presses;
        }
        if (to_press & K_LEFT) {
            move_left(ctrl_or_cmd, io.KeyShift);
            set_last_key_pressed(K_LEFT);
            m_start_suggesting = false;
        }
        else if (to_press & K_RIGHT) {
            move_right(ctrl_or_cmd, io.KeyShift);
            set_last_key_pressed(K_RIGHT);
            m_start_suggesting = false;
        }
        else if (to_press & K_UP) {
            if (m_start_suggesting) {
                if (m_search_highlight == 0) {
                    m_search_highlight = m_search_results.size() - 1;
                }
                else {
                    m_search_highlight--;
                }
            }
            else {
                move_up(io.KeyShift);
                set_last_key_pressed(K_UP);
            }
        }
        else if (to_press & K_DOWN) {
            if (m_start_suggesting) {
                if (m_search_highlight == m_search_results.size() - 1) {
                    m_search_highlight = 0;
                }
                else {
                    m_search_highlight++;
                }
            }
            else {
                move_down(io.KeyShift);
                set_last_key_pressed(K_DOWN);
            }
        }
        else if (to_press & K_HOME) {
            home(io.KeyShift);
            set_last_key_pressed(K_HOME);
            m_start_suggesting = false;
        }
        else if (to_press & K_END) {
            end(io.KeyShift);
            set_last_key_pressed(K_END);
            m_start_suggesting = false;
        }
        else if (to_press & K_BACKSPACE) {
            if (m_cursor_pos != m_cursor_selection_begin) {
                delete_at(m_cursor_selection_begin, m_cursor_pos, false);
            }
            else if (m_cursor_pos > 0) {
                if (ctrl_or_cmd)
                    delete_at(find_previous_word(m_cursor_pos), m_cursor_pos, false, FORCE);
                else
                    delete_at(m_cursor_pos - 1, m_cursor_pos, false);
            }
            m_start_suggesting = false;
        }
        else if (to_press & K_DEL) {
            if (m_cursor_pos != m_cursor_selection_begin) {
                delete_at(m_cursor_selection_begin, m_cursor_pos, false);
            }
            else if (m_cursor_pos < m_text.size()) {
                if (ctrl_or_cmd)
                    delete_at(m_cursor_pos, find_next_word(m_cursor_pos), false, FORCE);
                else
                    delete_at(m_cursor_pos, m_cursor_pos + 1, false);
            }
            m_start_suggesting = false;
        }
        else if (to_press & K_ENTER) {
            std::string to_insert = "\n";
            insert_with_selection(to_insert);
            m_start_suggesting = false;
        }
        else if (to_press & K_ESCAPE) {
            m_start_suggesting = false;
        }
        else if (io.InputQueueCharacters.Size > 0) {
            std::string to_insert;
            for (int n = 0; n < io.InputQueueCharacters.Size; n++) {
                unsigned int c = (unsigned int)io.InputQueueCharacters[n];
                if (c == '\t' && io.KeyShift)
                    continue;
                if (c == '\\')
                    m_start_suggesting = true;
                else if (!is_alphanum(c))
                    m_start_suggesting = false;
                to_insert += c;
            }
            insert_with_selection(to_insert);
            // Consume characters
            io.InputQueueCharacters.resize(0);
        }
        // Todo: use Tempo shortcut api
        else if (ctrl_or_cmd && ImGui::IsKeyPressed(ImGuiKey_A)) {
            m_cursor_selection_begin = 0;
            m_cursor_pos = m_text.size();
            m_cursor_last_hpos = m_cursor_pos - find_line_begin(m_cursor_pos);
            m_cursor_find_pos = true;
            m_start_suggesting = false;
        }
        else if (ctrl_or_cmd && ImGui::IsKeyPressed(ImGuiKey_D)) {
            select_word();
            m_start_suggesting = false;
        }
        else if (ctrl_or_cmd && ImGui::IsKeyPressed(ImGuiKey_D)) {
            select_word();
            m_start_suggesting = false;
        }
        else if (ctrl_or_cmd && ImGui::IsKeyPressed(ImGuiKey_C)) {
            ImGui::SetClipboardText(m_text.substr(m_cursor_selection_begin, m_cursor_pos - m_cursor_selection_begin).c_str());
            m_start_suggesting = false;
        }
        else if (ctrl_or_cmd && ImGui::IsKeyPressed(ImGuiKey_V)) {
            std::string to_insert = ImGui::GetClipboardText();
            insert_with_selection(to_insert);
            m_start_suggesting = false;
        }
        else if (ctrl_or_cmd && ImGui::IsKeyPressed(ImGuiKey_X)) {
            ImGui::SetClipboardText(m_text.substr(m_cursor_selection_begin, m_cursor_pos - m_cursor_selection_begin).c_str());
            delete_at(m_cursor_selection_begin, m_cursor_pos, false, FORCE);
            m_start_suggesting = false;
        }
        else if (ctrl_or_cmd && ImGui::IsKeyPressed(ImGuiKey_Y)
            || ctrl_or_cmd && io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_Z)) {
            if (m_history_idx < m_history.size() - 1) {
                m_history_idx++;
                set_text(m_history[m_history_idx].text);
                m_cursor_pos = m_history[m_history_idx].cursor_pos;
                m_cursor_selection_begin = m_cursor_pos;
                m_cursor_find_pos = true;
            }
            m_start_suggesting = false;
        }
        else if (ctrl_or_cmd && ImGui::IsKeyPressed(ImGuiKey_Z)) {
            if (m_history_idx > 0) {
                m_history_idx--;
                set_text(m_history[m_history_idx].text);
                m_cursor_pos = m_history[m_history_idx].cursor_pos;
                m_cursor_selection_begin = m_cursor_pos;
                m_cursor_find_pos = true;
            }
            m_start_suggesting = false;
        }
    }

    auto mouse_pos = ImGui::GetMousePos();
    auto cursor_pos = ImGui::GetCursorScreenPos();
    auto relative_pos = mouse_pos - cursor_pos;

    if (isInsideRect(mouse_pos, boundaries)) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_TextInput);
        if (ImGui::IsMouseClicked(0)) {
            m_is_focused = true;
            m_cursor_pos = coordinate_to_charpos(relative_pos);
            if (!io.KeyShift)
                m_cursor_selection_begin = m_cursor_pos;
            m_cursor_line_number = find_line_number(m_cursor_pos);
            m_cursor_last_hpos = m_cursor_pos - find_line_begin(m_cursor_pos);
            m_cursor_find_pos = true;
            m_start_dragging = true;
            m_start_suggesting = false;
        }
        else if (ImGui::IsMouseDoubleClicked(0)) {
            select_word();
        }
    }
    else {
        if (ImGui::IsMouseClicked(0) || ImGui::IsMouseClicked(1) || ImGui::IsMouseClicked(2)) {
            m_is_focused = false;
            m_start_suggesting = false;
        }
    }
    if (ImGui::IsMouseReleased(0)) {
        m_start_dragging = false;
    }
    if (m_start_dragging) {
        m_cursor_pos = coordinate_to_charpos(relative_pos);
        m_cursor_line_number = find_line_number(m_cursor_pos);
        m_cursor_last_hpos = m_cursor_pos - find_line_begin(m_cursor_pos);
        m_cursor_find_pos = true;
        m_start_suggesting = false;
    }
}


/* ===============================
 *            Parsing
 * =============================== */
void LatexEditor::make_history(HistoryAction action, const std::string& text_cpy, size_t cursor_pos, size_t change_size, char start_char) {
    HistoryPoint::InsertType insert_type = HistoryPoint::InsertType::NONE;
    if (is_alphanum(start_char))
        insert_type = HistoryPoint::InsertType::ALPHANUM;
    else if (is_whitespace(start_char))
        insert_type = HistoryPoint::InsertType::WHITESPACE;
    else if (is_special_char(start_char))
        insert_type = HistoryPoint::InsertType::SPECIAL;

    if (change_size > 1 || action == FORCE) {
        insert_type = HistoryPoint::InsertType::NONE;
    }

    if (m_history.size() > 0 && insert_type == m_history.back().insert_type && insert_type != HistoryPoint::InsertType::NONE) {
        m_history.erase(m_history.begin() + m_history_idx, m_history.end());
    }
    if (action != NONE) {
        m_history.push_back({ text_cpy,cursor_pos, insert_type });
        m_history_idx = m_history.size() - 1;
    }
}
void LatexEditor::insert_with_selection(const std::string& str) {
    bool was_selection = false;
    // Temporary fix for special chars in latex which makes the parser crash
    std::string new_str;
    for (unsigned char c : str) {
        if (c < 128)
            new_str += c;
    }

    if (new_str.empty())
        return;

    if (m_cursor_selection_begin != m_cursor_pos) {
        was_selection = true;
        delete_at(m_cursor_selection_begin, m_cursor_pos, true, NONE);
        if (m_cursor_pos > m_cursor_selection_begin)
            m_cursor_pos = m_cursor_selection_begin;
    }
    if (was_selection)
        insert_at(m_cursor_pos, new_str, false, FORCE);
    else
        insert_at(m_cursor_pos, new_str, false, GUESS);
}
void LatexEditor::insert_at(size_t pos, const std::string& str, bool skip_cursor_move, HistoryAction action) {
    char current_char = m_text[pos];
    if (pos > 0)
        current_char = m_text[pos - 1];

    m_text.insert(pos, str);
    m_has_text_changed = true;
    if (!skip_cursor_move) {
        m_cursor_pos += str.size();
        parse();
        m_cursor_selection_begin = m_cursor_pos;
        m_cursor_line_number = find_line_number(m_cursor_pos);
        m_cursor_last_hpos = m_cursor_pos - find_line_begin(m_cursor_pos);
        m_cursor_find_pos = true;
    }
    make_history(action, m_text, m_cursor_pos, str.size(), str[0]);
}
void LatexEditor::delete_at(size_t from, size_t to, bool skip_cursor_move, HistoryAction action) {
    if (from > to)
        std::swap(from, to);
    char start_char = m_text[from];
    char end_char = m_text[to];

    m_text.erase(from, to - from);
    m_has_text_changed = true;
    if (!skip_cursor_move) {
        parse();
        m_cursor_pos = from;
        m_cursor_selection_begin = m_cursor_pos;
        m_cursor_line_number = find_line_number(m_cursor_pos);
        m_cursor_last_hpos = m_cursor_pos - find_line_begin(m_cursor_pos);
        m_cursor_find_pos = true;
    }
    make_history(action, m_text, m_cursor_pos, to - from, start_char);
}
void LatexEditor::parse() {
    m_reparse = false;
    m_wrap_column.clear();
    m_line_positions.clear();
    m_commands.clear();
    m_openings_to_closings.clear();
    m_total_width = 0.f;
    m_total_height = 0.f;

    auto& ui_state = UIState::getInstance();
    Style style;
    style.font_styling = { Fonts::F_MONOSPACE, Fonts::W_REGULAR, Fonts::S_NORMAL };
    style.font_color = Colors::black;
    style.font_bg_color = Colors::transparent;

    size_t line_start = 0;
    size_t line_end = 0;
    size_t line_count = 0;
    bool is_prev_backslash = false;
    size_t bracket_counter = 0;
    size_t left_counter = 0;
    m_line_positions.insert(0);

    for (size_t i = 0;i < m_text.size();i++) {
        is_prev_backslash = false;
        if (m_text[i] == '\n') {
            line_end = i;
            line_start = i;
            line_count++;
            m_line_positions.insert(i + 1);
            auto& line = m_wrap_column[line_count];
            line.height = m_line_height * m_line_space;
        }
        else if (m_text[i] == '\\') {
            size_t command_start = i;
            i++;
            while (i < m_text.size() && is_alpha(m_text[i]))
                i++;
            if (i < m_text.size() && m_text[i] == '\\')
                i++;

            style.font_color = m_config.command_color;
            m_reparse |= !Utf8StrToImCharStr(ui_state, &m_wrap_column, m_text, line_count, command_start, i, style, false);
            i--;
        }
        else if (m_text[i] == '{' || m_text[i] == '}') {
            if (m_text[i] == '{')
                bracket_counter++;
            else
                bracket_counter--;
            style.font_color = m_config.bracket_color;
            m_reparse |= !Utf8StrToImCharStr(ui_state, &m_wrap_column, m_text, line_count, i, i + 1, style, false);
        }
        else {
            style.font_color = m_config.text_color;
            m_reparse |= !Utf8StrToImCharStr(ui_state, &m_wrap_column, m_text, line_count, i, i + 1, style, false);
        }
    }
    WrapAlgorithm wrapper;
    wrapper.setWidth(5000000, false);
    wrapper.setLineSpace(m_line_space, false);
    wrapper.setDefaultEmptyLineHeight(m_line_height * m_line_space, false);
    wrapper.setTextColumn(&m_wrap_column);
    m_total_height = wrapper.getHeight();
    for (auto& pair : m_wrap_column.getParagraph()) {
        auto& line = pair.second;
        if (line.sublines.size() > 0 && line.sublines.front().width > m_total_width) {
            m_total_width = line.sublines.front().width;
        }
    }
}

void LatexEditor::set_text(const std::string& text) {
    m_text = text;
    parse();
    m_cursor_pos = m_text.size() - 1;
    m_cursor_selection_begin = m_cursor_pos;
    m_cursor_line_number = find_line_number(m_cursor_pos);
    m_cursor_last_hpos = m_cursor_pos - find_line_begin(m_cursor_pos);
    m_cursor_find_pos = true;
}

/* ===============================
 *       Text cursor movement
 * =============================== */
void LatexEditor::set_cursor_idx(size_t pos, bool remove_selection) {
    m_cursor_pos = pos;
    if (remove_selection)
        m_cursor_selection_begin = m_cursor_pos;
    m_cursor_line_number = find_line_number(m_cursor_pos);
    m_cursor_last_hpos = m_cursor_pos - find_line_begin(m_cursor_pos);
    m_cursor_find_pos = true;
}
void LatexEditor::move_up(bool shift) {
    if (m_cursor_line_number > 0) {
        m_cursor_find_pos = true;
        m_cursor_pos = find_line_begin(m_cursor_pos);
        m_cursor_pos--;
        size_t line_begin = find_line_begin(m_cursor_pos);
        while (m_cursor_pos > 0 && m_cursor_last_hpos < m_cursor_pos - line_begin) {
            m_cursor_pos--;
        }
        m_cursor_line_number = find_line_number(m_cursor_pos);
    }
    else if (m_cursor_pos > 0) {
        m_cursor_pos = 0;
        m_cursor_find_pos = true;
        m_cursor_last_hpos = 0;
    }
    if (!shift)
        m_cursor_selection_begin = m_cursor_pos;
}
void LatexEditor::move_down(bool shift) {
    if (m_cursor_line_number < m_line_positions.size() - 1) {
        m_cursor_find_pos = true;
        m_cursor_pos = find_line_end(m_cursor_pos);
        m_cursor_pos++;
        size_t line_begin = m_cursor_pos;
        size_t line_end = find_line_end(m_cursor_pos);
        while (m_cursor_pos < m_text.size() && m_cursor_last_hpos > m_cursor_pos - line_begin && m_cursor_pos < line_end) {
            m_cursor_pos++;
        }
        if (m_cursor_pos > m_text.size()) {
            m_cursor_pos--;
        }
        m_cursor_line_number = find_line_number(m_cursor_pos);
    }
    else if (m_cursor_pos < m_text.size()) {
        m_cursor_pos = m_text.size();
        m_cursor_last_hpos = m_cursor_pos - find_line_begin(m_cursor_pos);
        m_cursor_find_pos = true;
    }
    if (!shift)
        m_cursor_selection_begin = m_cursor_pos;
}
void LatexEditor::move_left(bool word, bool shift) {
    if (m_cursor_pos > 0) {
        if (m_cursor_selection_begin != m_cursor_pos && !shift) {
            if (m_cursor_pos > m_cursor_selection_begin)
                m_cursor_pos = m_cursor_selection_begin;
            else
                m_cursor_selection_begin = m_cursor_pos;
        }
        else {
            if (m_line_positions.find(m_cursor_pos) != m_line_positions.end()) {
                m_cursor_pos--;
            }
            else if (word) {
                m_cursor_pos = find_previous_word(m_cursor_pos);
            }
            else {
                m_cursor_pos--;
            }
            m_cursor_line_number = find_line_number(m_cursor_pos);
            m_cursor_last_hpos = m_cursor_pos - find_line_begin(m_cursor_pos);
            m_cursor_find_pos = true;
        }
    }
    if (!shift)
        m_cursor_selection_begin = m_cursor_pos;
}
void LatexEditor::move_right(bool word, bool shift) {
    if (m_cursor_pos < m_text.size()) {
        if (m_cursor_selection_begin != m_cursor_pos && !shift) {
            if (m_cursor_pos > m_cursor_selection_begin)
                m_cursor_pos = m_cursor_selection_begin;
            else
                m_cursor_selection_begin = m_cursor_pos;
        }
        else {
            if (m_line_positions.find(m_cursor_pos + 1) != m_line_positions.end()) {
                m_cursor_pos++;
            }
            else if (word) {
                m_cursor_pos = find_next_word(m_cursor_pos);
            }
            else {
                m_cursor_pos++;
            }
            m_cursor_line_number = find_line_number(m_cursor_pos);
            m_cursor_last_hpos = m_cursor_pos - find_line_begin(m_cursor_pos);
            m_cursor_find_pos = true;
        }
    }
    if (!shift)
        m_cursor_selection_begin = m_cursor_pos;
}
void LatexEditor::home(bool shift) {
    bool skip_whitespace = true;
    if (m_last_key_pressed == K_HOME) {
        skip_whitespace = false;
    }
    m_cursor_pos = find_home(m_cursor_pos, skip_whitespace);
    m_cursor_last_hpos = m_cursor_pos - find_line_begin(m_cursor_pos);
    m_cursor_find_pos = true;
    if (!shift)
        m_cursor_selection_begin = m_cursor_pos;
}
void LatexEditor::end(bool shift) {
    bool skip_whitespace = true;
    m_cursor_pos = find_end(m_cursor_pos, skip_whitespace);
    m_cursor_last_hpos = m_cursor_pos - find_line_begin(m_cursor_pos);
    m_cursor_find_pos = true;
    if (!shift)
        m_cursor_selection_begin = m_cursor_pos;
}
void LatexEditor::select_word() {
    m_cursor_selection_begin = find_previous_word(m_cursor_pos);
    m_cursor_pos = find_next_word(m_cursor_pos);
    m_cursor_last_hpos = m_cursor_pos - find_line_begin(m_cursor_pos);
    m_cursor_find_pos = true;
}


/* ===============================
 *     Text cursor primitives
 * =============================== */
size_t LatexEditor::find_next_word(size_t pos) {
    if (is_whitespace(m_text[pos])) {
        while (pos < m_text.size() && is_whitespace(m_text[pos]))
            pos++;
    }
    if (is_alphanum(m_text[pos])) {
        while (pos < m_text.size() && is_alphanum(m_text[pos]))
            pos++;
    }
    else {
        while (pos < m_text.size() && !is_alphanum(m_text[pos]) && !is_whitespace(m_text[pos]))
            pos++;
    }
    return pos;
}
size_t LatexEditor::find_previous_word(size_t pos) {
    if (pos < 2)
        return 0;

    pos--;
    if (is_whitespace(m_text[pos])) {
        while (pos > 0 && is_whitespace(m_text[pos]))
            pos--;
    }
    if (is_alphanum(m_text[pos])) {
        while (pos > 0 && is_alphanum(m_text[pos]))
            pos--;
    }
    else {
        while (pos > 0 && !is_alphanum(m_text[pos]) && !is_whitespace(m_text[pos]))
            pos--;
    }
    if (pos > 0)
        pos++;
    return pos;
}
size_t LatexEditor::find_home(size_t pos, bool skip_whitespace) {
    size_t line_begin = find_line_begin(pos);
    size_t line_end = find_line_end(pos);
    pos = line_begin;
    if (skip_whitespace) {
        while (is_whitespace(m_text[pos]) && pos <= line_end)
            pos++;
        if (pos == line_end)
            pos = line_begin;
    }
    return pos;
}
size_t LatexEditor::find_end(size_t pos, bool) {
    size_t line_begin = find_line_begin(pos);
    size_t line_end = find_line_end(pos);
    pos = line_end;
    return pos;
}

/* ===============================
 *             Drawing
 * =============================== */
void LatexEditor::set_std_char_info() {
    if (!m_is_char_info_set) {
        WrapColumn col;
        std::string test_string = "abc|&g";
        auto& ui_state = UIState::getInstance();
        Style style;
        m_is_char_info_set |= Utf8StrToImCharStr(ui_state, &col, test_string, 0, 0, 2, style, false);
        if (m_is_char_info_set) {
            WrapAlgorithm wrapper;
            wrapper.setWidth(5000000, false);
            wrapper.setLineSpace(m_line_space, false);
            wrapper.setTextColumn(&col);
            auto& subline = col[0].sublines.front();
            m_line_height = subline.max_ascent + subline.max_descent;
            m_advance = col[0].chars.front()->info->advance;
        }
    }
}

ImVec2 LatexEditor::locate_char_coord(size_t pos, bool half_line_space) {
    size_t line_number = find_line_number(pos);
    auto& line = m_wrap_column[line_number];
    bool found_next_char = false;
    float last_x_pos = 0.f;
    ImVec2 out_pos(0, line.relative_y_pos);
    for (auto ch : line.chars) {
        if (ch->text_position > pos) {
            out_pos.x = ch->calculated_position.x;
            found_next_char = true;
            break;
        }
        last_x_pos = ch->calculated_position.x + ch->info->advance;
    }
    if (!found_next_char) {
        out_pos.x = last_x_pos;
    }
    if (half_line_space) {
        out_pos.y -= m_line_height * (m_line_space - 1.f) / 2.f;
    }
    return out_pos;
}
void LatexEditor::draw_cursor() {
    if (!m_is_focused)
        return;

    if (m_cursor_find_pos) {
        if (m_wrap_column.find(m_cursor_line_number) != m_wrap_column.end()) {
            m_cursor_find_pos = false;
            m_cursor_drawpos = locate_char_coord(m_cursor_pos, true);
        }
        else if (m_text.empty()) {
            m_cursor_find_pos = false;
            m_cursor_drawpos = ImVec2(0, 0);
        }
    }
    ImVec2 pos = ImGui::GetCursorScreenPos() + m_cursor_drawpos;
    pos.x -= 1.f;
    ImVec2 end_pos = pos;
    end_pos.y += m_line_height * m_line_space;
    m_draw_list->AddLine(pos, end_pos, m_config.cursor_color, 1.f);
}
void LatexEditor::draw_decoration(ImVec2 char_p1, ImVec2 char_p2, const CharDecoInfo& decoration) {
    if (decoration.type == CharDecoInfo::BACKGROUND)
        m_draw_list->AddRectFilled(char_p1, char_p2, decoration.color);
    else if (decoration.type == CharDecoInfo::UNDERLINE) {
        char_p1.y = char_p2.y;
        m_draw_list->AddLine(char_p1, char_p2, decoration.color, decoration.thickness.getFloat());
    }
    else if (decoration.type == CharDecoInfo::BOX) {
        m_draw_list->AddRect(char_p1, char_p2, decoration.color, 0.f, 0, decoration.thickness.getFloat());
    }
    else if (decoration.type == CharDecoInfo::SQUIGLY) {
        float x = char_p1.x;
        float y = char_p2.y;
        float w = char_p2.x - char_p1.x;
        float step = emfloat{ 4.f }.getFloat();
        float step_count = w / step;
        float step_height = emfloat{ 1.5f }.getFloat();
        float step_width = w / step_count;
        float current_y_step = step_height;
        for (float i = 0.f;i < step_count;i++) {
            if (x > char_p2.x)
                break;
            m_draw_list->AddLine(ImVec2(x, y - current_y_step), ImVec2(x + step_width, y + current_y_step), decoration.color, decoration.thickness.getFloat());
            x += step_width;
            current_y_step = -current_y_step;
        }
    }
}
void LatexEditor::draw_suggestions() {
    if (!m_start_suggesting)
        return;
    std::string query;
    size_t i = m_cursor_pos - 1;
    while (i >= 0) {
        query += m_text[i];
        if (m_text[i] == '\\')
            break;
        i--;
    }
    size_t start_pos = i;
    std::reverse(query.begin(), query.end());
    if (query != m_query && query.size() > 1) {
        m_search_results = m_search_commands.getBestSuggestions(query);
        m_search_highlight = 0;
    }
    if (m_search_results.size() > 0) {
        Fonts::FontInfoOut fout;
        UIState::getInstance().font_manager.requestFont({ Fonts::F_MONOSPACE, Fonts::W_REGULAR, Fonts::S_NORMAL }, fout);
        Tempo::PushFont(fout.font_id);
        int num_results = m_search_results.size();
        auto& style = ImGui::GetStyle();
        float item_height = ImGui::GetTextLineHeight() + style.FramePadding.y * 2.0f;
        float window_height = item_height * (float)MIN(MAX(1, num_results), 5) + style.FramePadding.y * 2.0f;

        ImVec2 pos = ImGui::GetCursorScreenPos() + m_cursor_drawpos;
        pos.x -= 1.f;
        ImVec2 end_pos = pos;
        end_pos.y += m_line_height * m_line_space;
        ImGui::SetNextWindowPos(end_pos);
        ImGui::SetNextWindowSize(ImVec2(0, window_height), ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.9f);
        static bool show = true;

        ImGui::Begin("#Search results", &show,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoDocking
            | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_AlwaysAutoResize);
        int i = 0;
        for (auto& command : m_search_results) {
            ImGui::SetNextWindowFocus();
            if (m_search_highlight == i && ImGui::IsKeyPressed(ImGuiKey_Tab)) {
                show = false;
                m_start_suggesting = false;
                delete_at(start_pos, m_cursor_pos, true, LatexEditor::HistoryAction::NONE);
                insert_at(start_pos, command.str, false, LatexEditor::HistoryAction::FORCE);
                set_cursor_idx(start_pos + command.indices[0], true);
            }
            // ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, ImGui::GetStyleColorVec4(ImGuiCol_FrameBgHovered));
            ImGui::Selectable(command.str.c_str(), m_search_highlight == i);
            // ImGui::PopStyleColor();
            i++;
        }
        Tempo::PopFont();
        ImGui::End();
    }
    m_query = query;

}
void LatexEditor::char_decoration(size_t from, size_t to, const std::vector<CharDecoInfo>& decorations) {
    ImVec2 highlight_from;
    ImVec2 highlight_to;
    bool start_selection = false;
    auto screen_pos = ImGui::GetCursorScreenPos();
    for (size_t i = from; i < to;i++) {
        if (!start_selection) {
            start_selection = true;
            highlight_from = locate_char_coord(i, true);
        }
        if (m_text[i] == '\n') {
            start_selection = false;
            highlight_to = locate_char_coord(i, true);
            highlight_to.y += m_line_height * m_line_space;
            highlight_from += screen_pos;
            highlight_to += screen_pos;
            // Indicate to the user that he selected a \n
            highlight_to.x += m_advance / 3.f;
            for (const auto& deco : decorations) {
                draw_decoration(highlight_from, highlight_to, deco);
            }
        }
    }
    if (start_selection) {
        highlight_to = locate_char_coord(to, true);
        highlight_to.y += m_line_height * m_line_space;
        highlight_from += screen_pos;
        highlight_to += screen_pos;
        for (const auto& deco : decorations) {
            draw_decoration(highlight_from, highlight_to, deco);
        }
    }
}

size_t LatexEditor::coordinate_to_charpos(const ImVec2& relative_coordinate) {
    if (m_wrap_column.empty())
        return 0;
    size_t line_number = 0;
    for (auto& pair : m_wrap_column) {
        if (pair.second.relative_y_pos > relative_coordinate.y)
            break;
        line_number++;
    }
    if (line_number > 0)
        line_number--;
    auto& line = m_wrap_column[line_number];
    float last_x_pos = 0.f;
    size_t position = 0;
    for (auto ch : line.chars) {
        if (ch->calculated_position.x + ch->info->advance / 2.f > relative_coordinate.x) {
            break;
        }
        position = ch->text_position;
    }
    return position;
}

void LatexEditor::debug_window() {
    if (m_config.debug) {
        ImGui::Begin("Debug");
        ImGui::Text("Cursor pos: %d", m_cursor_pos);
        ImGui::Text("Cursor line: %d", m_cursor_line_number);
        ImGui::Text("Cursor last h pos: %d", m_cursor_last_hpos);
        ImGui::Text("Is focused: %d", m_is_focused);
        ImGui::Text("Text size: %d", m_text.size());
        ImGui::Text("Width %f height %f", m_total_width, m_total_height);
        if (ImGui::TreeNode("History")) {
            for (auto it = m_history.rbegin();it != m_history.rend();it++) {
                ImGui::BulletText("%s", it->text.c_str());
            }
            ImGui::TreePop();
        }
        ImGui::End();
    }
}

void LatexEditor::draw(std::string& latex, ImVec2 size) {
    set_std_char_info();
    debug_window();

    if (size.y == 0) {
        size.y = emfloat{ 150 }.getFloat();
    }
    float set_x = size.x;
    if (size.x == 0) {
        size.x = ImGui::GetContentRegionAvail().x;
    }
    auto pos = ImGui::GetCursorScreenPos();
    ImGui::BeginChild("latex_editor", size, true, ImGuiWindowFlags_HorizontalScrollbar);
    if (m_reparse) {
        parse();
    }
    // TODO: fix boundaries
    Rect boundaries;
    boundaries.x = pos.x;
    boundaries.y = pos.y;
    boundaries.w = ImGui::GetContentRegionAvail().x;
    boundaries.h = ImGui::GetContentRegionAvail().y;
    events(boundaries);
    if (m_reparse) {
        parse();
    }

    m_draw_list.SetImDrawList(ImGui::GetWindowDrawList());

    size_t min_selection = std::min(m_cursor_pos, m_cursor_selection_begin);
    size_t max_selection = std::max(m_cursor_pos, m_cursor_selection_begin);
    draw_cursor();
    char_decoration(min_selection, max_selection, { { CharDecoInfo::BACKGROUND, m_config.selection_color } });

    for (auto& pair : m_wrap_column) {
        ImVec2 pos;
        for (auto& ptr : pair.second.chars) {
            auto p = std::static_pointer_cast<DrawableChar>(ptr);
            p->draw(&m_draw_list, boundaries, pos);
        }
    }
    draw_suggestions();
    ImGui::SetCursorPos(ImVec2(m_total_width, m_total_height));
    ImGui::EndChild();
}

std::string LatexEditor::get_text() {
    return m_text;
}
bool LatexEditor::has_text_changed() {
    if (m_has_text_changed) {
        m_has_text_changed = false;
        return true;
    }
    else
        return false;
}