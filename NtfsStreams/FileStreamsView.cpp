#include "pch.h"
#include "FileStreamsView.h"
#include "Helpers.h"

CString CFileStreamsView::GetTitle() const {
	return Helpers::FileName(m_Path).c_str();
}

void CFileStreamsView::OnPanesStatusChanged() {
	if (m_Active)
		Frame()->SetStatusText(m_Panes.GetStatusText().c_str());
}

LRESULT CFileStreamsView::OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/) {
	m_hWndClient = m_Panes.DoCreate(m_hWnd);
	m_Panes.ShowFile(m_Path, m_IsDirectory);
	return 0;
}
