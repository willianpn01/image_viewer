#pragma once
#define cimg_display 0
#include "CImg.h"
#include <vector>
#include <string>
#include <utility>

class UndoStack {
public:
    using Image = cimg_library::CImg<unsigned char>;

    explicit UndoStack(int maxDepth = 20);

    // Push current state onto the stack (call AFTER applying the operation)
    void push(const std::string& opName, const Image& state);

    bool canUndo() const;
    bool canRedo() const;

    // Returns the state to restore
    Image undo();
    Image redo();

    void clear();

    // Name of the operation that would be undone/redone
    std::string nextUndoName() const;
    std::string nextRedoName() const;

    int size() const;

private:
    std::vector<std::pair<std::string, Image>> m_history;
    int m_current = -1;
    int m_maxDepth;
};
