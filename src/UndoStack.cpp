#include "UndoStack.hpp"
#include <algorithm>

UndoStack::UndoStack(int maxDepth) : m_maxDepth(maxDepth) {}

void UndoStack::push(const std::string& opName, const Image& state) {
    // Erase redo history
    if (m_current < (int)m_history.size() - 1)
        m_history.erase(m_history.begin() + m_current + 1, m_history.end());

    m_history.emplace_back(opName, state);
    m_current = (int)m_history.size() - 1;

    // Trim to max depth
    while ((int)m_history.size() > m_maxDepth) {
        m_history.erase(m_history.begin());
        --m_current;
    }
}

bool UndoStack::canUndo() const { return m_current > 0; }
bool UndoStack::canRedo() const { return m_current < (int)m_history.size() - 1; }

UndoStack::Image UndoStack::undo() {
    if (!canUndo()) return Image();
    --m_current;
    return m_history[m_current].second;
}

UndoStack::Image UndoStack::redo() {
    if (!canRedo()) return Image();
    ++m_current;
    return m_history[m_current].second;
}

void UndoStack::clear() {
    m_history.clear();
    m_current = -1;
}

std::string UndoStack::nextUndoName() const {
    if (!canUndo()) return "";
    return m_history[m_current].first;
}

std::string UndoStack::nextRedoName() const {
    if (!canRedo()) return "";
    return m_history[m_current + 1].first;
}

int UndoStack::size() const { return (int)m_history.size(); }
