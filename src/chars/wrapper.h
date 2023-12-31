#pragma once

#include <vector>
#include <list>
#include <tempo.h>
#include <unordered_map>
#include <unordered_set>

#include "char.h"

struct WrapCharacter {
    virtual ~WrapCharacter() = default;

    Fonts::Character* info;
    ImVec2 calculated_position;
    int text_position = -1;
};

typedef std::shared_ptr<WrapCharacter> WrapCharPtr;
typedef std::weak_ptr<WrapCharacter> WrapCharWPtr;

typedef std::vector<WrapCharPtr> WrapString;

/**
 * @brief The convention for line positions is as follow:
 *
 * Let's use the following text as an example. Intentional line breaks
 * have been marked as \n, auto wrapped line breaks are marked as \nn
 *  ____________________________________
 * | This text is a dummy example of\nn | Line 1, position 0 - 31
 * | a wrapped line break.\n            | Line 2, position 31 - 52
 * | \n                                 | Line 3, position 53 - 53
 * | The text ends here.                | Line 4, position 54 - 73
 *  ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾
 * \n does count as a char, but \nn doesn't (as it is dynamically calculated)
 *
 * \nn line break positions: 31
 *  \n line break positions: 52, 53
 */
struct SubLine {
    int start;
    float height = 0.f;
    float width = 0.f;
    float rel_y_pos;
    float max_ascent = 0.f;
    float max_descent = 0.f;
};

struct WrapLine {
    std::vector<SubLine> sublines;
    WrapString chars;
    float relative_y_pos = 0.f;
    float height = 0.f;
};

class WrapColumn {
private:
    std::map<int, WrapLine> m_lines;
public:
    void push_back(const WrapCharPtr& ptr, int line) {
        if (m_lines.find(line) == m_lines.end()) {
            m_lines[line] = WrapLine();
        }
        m_lines[line].chars.push_back(ptr);
    }
    void clear() { m_lines.clear(); }
    bool empty() const { return m_lines.empty(); }
    std::map<int, WrapLine>::iterator begin() { return m_lines.begin(); }
    std::map<int, WrapLine>::iterator end() { return m_lines.end(); }
    std::map<int, WrapLine>::iterator find(int line) { return m_lines.find(line); }
    WrapLine& operator[](int line) { return m_lines[line]; }
    WrapLine& at(int line) { return m_lines.at(line); }

    std::map<int, WrapLine>& getParagraph() { return m_lines; }
};

class AbstractElement;
typedef AbstractElement* AbstractElementPtr;

/**
 * @brief Only handles wrapping and scrolling
 */
class WrapAlgorithm {
private:
    WrapColumn* m_text_column = nullptr;
    WrapString* m_current_string;

    // Calculated quantities
    std::list<SubLine> m_sublines;
    std::set<int> m_line_positions;
    float m_total_height;

    // User set quantities
    std::vector<float> m_widths = { 1.f };
    float m_width;
    float m_x_offset = 0.f;
    float m_current_height = 0.f;
    float m_line_space = 1.3f;
    float m_first_max_ascent = 0.f;
    float m_first_max_descent = 0.f;
    float m_default_empty_line_height = 15.f;

    /**
     * @brief Returns the index of the line which contains pos
     */
    inline int find_line_idx(int cursor_pos);
    inline int find_next_line_break(int cursor_pos);

    inline void push_char_on_line(WrapCharPtr c, float* cursor_x_coord);
    inline void push_new_line(std::list<SubLine>::iterator& line_it, int cursor_pos, float* cursor_x_coord);

    void algorithm();
public:
    /**
     * @brief Construct a new Text Wrapper object
     *
     * @param width width (in px) of the box
     * @param line_space relative line space: space between lines is calculated as line_height * line_space
     */
    WrapAlgorithm(float width, float line_space = 1.3);
    WrapAlgorithm();
    ~WrapAlgorithm();

    // void setString(WrapString* string, bool redo = true);
    void setTextColumn(WrapColumn* paragraph, bool redo = true);
    void clear();
    void recalculate();
    void recalculate(WrapLine* line, float x_offset = 0.f);
    float getHeight() { return m_current_height; }
    float getFirstMaxAscent() { return m_first_max_ascent; }
    float getFirstMaxDescent() { return m_first_max_descent; }

    void setDefaultEmptyLineHeight(float height, bool redo = true);
    void setWidth(float width, bool redo = true);
    void setWidth(const std::vector<float>& width, bool redo = true);
    void setLineSpace(float line_space, bool redo = true);
};