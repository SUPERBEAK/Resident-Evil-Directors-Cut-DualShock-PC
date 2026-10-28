#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <d3d12.h>

struct Document {
    uint8_t documentId;
    std::string title;
    std::vector<std::string> pages;
};

class DocumentViewer {
public:
    DocumentViewer() = default;
    ~DocumentViewer() = default;

    bool IsOpen() const { return m_isOpen; }
    int CurrentPage() const { return m_currentPage; }
    int TotalPages() const {
        if (m_currentDoc < m_documents.size()) {
            return static_cast<int>(m_documents[m_currentDoc].pages.size());
        }
        return 0;
    }

    void OpenDocument(uint8_t documentId);
    void Update(uint32_t padButtons, uint32_t pressedButtons);
    void Close();
    bool IsActive() const { return m_isOpen; }

    void Render(ID3D12GraphicsCommandList* cmdList);

private:
    bool m_isOpen = false;
    size_t m_currentDoc = 0;
    int m_currentPage = 0;
    std::vector<Document> m_documents;

    void InitDocuments();
    void DrawParchmentFrame();
    void DrawPageIndicator();
    void DrawText(const std::string& text);
    void UpdateControls();
};
