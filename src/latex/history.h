#pragma once

#include <string>
#include <vector>
#include <map>

#include "latex.h"
#include "core/basic.h"

struct LatexHistory {
    std::string latex;
    float aspect_ratio;
    std::string name;
    uint64_t timepoint = 0;
};
class History {
private:
    std::set<uint64_t> m_bookmarks;
    std::set<uint64_t> m_to_erase;
    std::map<uint64_t, LatexHistory> m_history;
    std::set<std::string> m_equations;

    uint64_t m_to_retrieve = 0;

    std::map<uint64_t, std::unique_ptr<Latex::LatexImage>> m_history_images;

    bool m_is_loaded = false;
    bool m_open = false;

    void save_to_file();
    void load();
    void clean();

    void show_history_point(const LatexHistory& history_point, const Rect& boundaries);
public:
    void saveToHistory(LatexHistory history_point);
    void saveBookmark(uint64_t timepoint, const std::string& name);

    std::vector<LatexHistory> getHistory();
    std::vector<LatexHistory> getBookmarks();

    void draw();
    void show() { m_open = true; }
    bool is_open() { return m_open; }
    bool must_retrieve_latex(LatexHistory& history_point);
};