#ifndef WXFLUIDCOLUMNLAYOUTMANAGER_HPP
#define WXFLUIDCOLUMNLAYOUTMANAGER_HPP

#include <vector>
#include <memory>

#include <wx/window.h>

class wxListCtrl;
class wxGrid;
class wxSizeEvent;
class wxListCtrlEx;

class FluidColumnLayoutManager: public wxWindow {
public:
	class Impl;
	class ListImpl;
	class GridImpl;

	FluidColumnLayoutManager(wxListCtrlEx *list, bool persist = false);
	FluidColumnLayoutManager(wxGrid *grid, bool persist = false);

	~FluidColumnLayoutManager() override;

	long SetColumnWeight(long col, size_t weight);

private:
	friend ListImpl;
	friend GridImpl;

	void DistributeWidth(int width_to_distribute, int resizing_col = -1, int width_resizing_col = 0);

	void onColBeginDrag(int col);
	void onColDragging(int col, int col_width);
	void onColEndDrag(int col, int col_width);
	void resizeBecauseOfDragging(int col, int col_width);
	void resize();

private:
	std::unique_ptr<Impl> impl_;

	std::vector<size_t> weights_{};
	std::vector<int> cols_min_widths_{};
	std::vector<int> *persist_col_min_widths_{};
	std::vector<int> *persist_col_widths_{};

	int old_client_width_ = -1;
	int start_col_ = -1;
	int col_dragging_ = -1;
	bool allow_beyond_window_limits_when_dragging_ = false;
};

#endif // WXFLUIDCOLUMNLAYOUTMANAGER_HPP
