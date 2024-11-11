#include <libfilezilla/glue/wx.hpp>

#include <algorithm>
#include <numeric>

#include <wx/listctrl.h>
#include <wx/grid.h>
#include <wx/headerctrl.h>

#include "fluidcolumnlayoutmanager.hpp"
#include "locale.hpp"
#include "settings.hpp"

class FluidColumnLayoutManager::Impl
{
public:
	Impl(FluidColumnLayoutManager *owner)
		: owner_{owner}
	{}

	virtual ~Impl() = default;

	virtual int GetColumnWidth(int col) const = 0;
	virtual void SetColumnWidth(int col, int width) = 0;
	virtual int GetColumnCount() const = 0;
	virtual int GetClientWidth() const = 0;

protected:
	FluidColumnLayoutManager *owner_;
};

class FluidColumnLayoutManager::ListImpl: public Impl
{
public:
	ListImpl(FluidColumnLayoutManager *owner, wxListCtrlEx *list)
		: Impl{owner}
		, list_{list}
	{

		list->Bind(wxEVT_LIST_COL_BEGIN_DRAG, &ListImpl::onColBeginDrag, this);
		list->Bind(wxEVT_LIST_COL_DRAGGING, &ListImpl::onColDragging, this);
		list->Bind(wxEVT_LIST_COL_END_DRAG, &ListImpl::onColEndDrag, this);

		// We want to avoid to resize during the time wx does its own internal things and before anything is displayed on screen,
		// so to avoid any spurious drift of the columns. Hence, we allow resizing only if the window is already displayed.
		// This is true if IsShownOnScreen() returns true, or if a PAINT event is received.
		list->Bind(wxEVT_SIZE, [this](wxSizeEvent &ev) {
			ev.Skip();

			if (!can_resize_)
				can_resize_ = list_->IsShownOnScreen();

			if (can_resize_)
				owner_->CallAfter([this]{owner_->resize();});
		});

		list->GetMainWindow()->Bind(wxEVT_PAINT, [this](wxPaintEvent &ev) {
			ev.Skip();

			if (!can_resize_) {
				can_resize_ = true;
				list_->PostSizeEvent();
			}
		});
	}

private:
	wxListCtrl *list_;
	bool can_resize_{};

	void onColBeginDrag(wxListEvent &ev)
	{
		ev.Skip();
		owner_->onColBeginDrag(ev.GetColumn());
	}

	void onColDragging(wxListEvent &ev)
	{
		ev.Skip();

		// On Windows, the column width gets updated only after this event is executed,
		// Therefore postpone the execution of the action.
		owner_->CallAfter([this, col = ev.GetColumn()]{
			owner_->onColDragging(col, GetColumnWidth(col));
		});
	}

	void onColEndDrag(wxListEvent &ev)
	{
		ev.Skip();

		// On Windows, the column width gets updated only after this event is executed,
		// Therefore postpone the execution of the action.
		owner_->CallAfter([this, col = ev.GetColumn()]{
			owner_->onColEndDrag(col, GetColumnWidth(col));
		});
	}

	int GetColumnWidth(int col) const override {
		return list_->GetColumnWidth(col);
	}

	void SetColumnWidth(int col, int width) override {
		list_->SetColumnWidth(col, width);
	}

	int GetColumnCount() const override {
		return list_->GetColumnCount();
	}

	int GetClientWidth() const override {
		return list_->GetClientSize().GetWidth();
	}
};

class FluidColumnLayoutManager::GridImpl: public Impl
{
public:
	GridImpl(FluidColumnLayoutManager *owner, wxGrid *grid)
		: Impl{owner}
		, grid_{grid}
	{
		auto header = grid_->GetGridColHeader();

		header->Bind(wxEVT_HEADER_BEGIN_RESIZE, &GridImpl::onColBeginDrag, this);
		header->Bind(wxEVT_HEADER_RESIZING, &GridImpl::onColDragging, this);
		header->Bind(wxEVT_HEADER_END_RESIZE, &GridImpl::onColEndDrag, this);

		// We want to avoid to resize during the time wx does its own internal things and before anything is displayed on screen,
		// so to avoid any spurious drift of the columns. Hence, we allow resizing only if the window is already displayed.
		// This is true if IsShownOnScreen() returns true, or if a PAINT event is received.
		grid->Bind(wxEVT_SIZE, [this](wxSizeEvent &ev) {
			ev.Skip();

			if (!can_resize_)
				can_resize_ = grid_->IsShownOnScreen();

			if (can_resize_)
				owner_->CallAfter([this]{owner_->resize();});
		});

		header->Bind(wxEVT_PAINT, [this](wxPaintEvent &ev) {
			ev.Skip();

			if (!can_resize_) {
				can_resize_ = true;
				grid_->PostSizeEvent();
			}
		});
	}

private:
	wxGrid *grid_;
	bool can_resize_{};

	void onColBeginDrag(wxHeaderCtrlEvent &ev)
	{
		ev.Skip();
		owner_->onColBeginDrag(ev.GetColumn());
	}

	void onColDragging(wxHeaderCtrlEvent &ev)
	{
		ev.Skip();
		owner_->onColDragging(ev.GetColumn(), ev.GetWidth());
	}

	void onColEndDrag(wxHeaderCtrlEvent &ev)
	{
		ev.Skip();
		owner_->onColEndDrag(ev.GetColumn(), ev.GetWidth());
	}

	int GetColumnWidth(int col) const override {
		return grid_->GetColSize(col);
	}

	void SetColumnWidth(int col, int width) override {
		grid_->SetColSize(col, width);

		if (grid_->IsCellEditControlEnabled() && grid_->GetGridCursorCol() == col) {
			auto row = grid_->GetGridCursorRow();

			auto editor = grid_->GetCellEditor(row, col);
			if (editor->IsCreated())
				editor->SetSize(grid_->CellToRect(row, col));
			editor->DecRef();
		}
	}

	int GetColumnCount() const override {
		return grid_->GetNumberCols();
	}

	int GetClientWidth() const override {
		return grid_->GetClientSize().GetWidth() - grid_->GetRowLabelSize();
	}
};

FluidColumnLayoutManager::FluidColumnLayoutManager(wxListCtrlEx *list, bool persist)
	: impl_{new ListImpl(this, list)}
{
	if (persist) {
		persist_col_min_widths_ = &Settings()->columns()[fz::to_wstring(list->GetName())].min_widths;
		persist_col_widths_ = &Settings()->columns()[fz::to_wstring(list->GetName())].widths;
	}

	SetExtraStyle(GetExtraStyle() | wxWS_EX_TRANSIENT);
	Create(list, wxID_ANY);
	Hide();
}

FluidColumnLayoutManager::FluidColumnLayoutManager(wxGrid *grid, bool persist)
	: impl_{new GridImpl(this, grid)}
{
	if (persist) {
		persist_col_min_widths_ = &Settings()->columns()[fz::to_wstring(grid->GetName())].min_widths;
		persist_col_widths_ = &Settings()->columns()[fz::to_wstring(grid->GetName())].widths;
	}

	SetExtraStyle(GetExtraStyle() | wxWS_EX_TRANSIENT);
	Create(grid, wxID_ANY);
	Hide();
}

FluidColumnLayoutManager::~FluidColumnLayoutManager()
{
	if (persist_col_min_widths_) {
		*persist_col_min_widths_ = std::move(cols_min_widths_);

		Settings().Save();
	}
}

long FluidColumnLayoutManager::SetColumnWeight(long col, size_t weight)
{
	if (col >= 0) {
		if (weights_.size() < (size_t)col+1)
			weights_.resize((size_t)col+1);

		weights_[(size_t)col] = weight;
	}

	return col;
}

void FluidColumnLayoutManager::DistributeWidth(int width_to_distribute, int resizing_col, int width_resizing_col) {
	wxCHECK_RET((weights_.size() > 0),
				_S("You must add some columns first"));

	auto sum_of_weights = std::accumulate(weights_.begin(), weights_.end(), 0);

	auto find_next_weighted_col = [&, count = size_t{0}](int col) mutable {
		while (count++ < weights_.size()) {
			(col += 1) %= weights_.size();

			if (weights_[(size_t)col] > 0)
				return col;
		}

		count = 0;
		return -1;
	};

	start_col_ = find_next_weighted_col(start_col_);

	for (auto col = start_col_; col != -1; col = find_next_weighted_col(col)) {
		auto weight = weights_[(size_t)col];

		auto old_col_width = resizing_col == col ? width_resizing_col : impl_->GetColumnWidth(col);
		int col_extra_width = (int)std::round(width_to_distribute*(double)weight/(double)sum_of_weights);
		auto new_col_width = old_col_width + col_extra_width;

		if (new_col_width < cols_min_widths_[(size_t)col]) {
			new_col_width = cols_min_widths_[(size_t)col];
			col_extra_width = old_col_width - new_col_width;
		}

		if (new_col_width != old_col_width) {
			impl_->SetColumnWidth(col, new_col_width);
			width_to_distribute -= col_extra_width;

			if (persist_col_widths_)
				(*persist_col_widths_)[std::size_t(col)] = new_col_width;
		}

		sum_of_weights -= weight;
	}
}


void FluidColumnLayoutManager::onColBeginDrag(int col)
{
	col_dragging_ = col;
	allow_beyond_window_limits_when_dragging_ = wxGetKeyState(WXK_CONTROL);
}

void FluidColumnLayoutManager::onColEndDrag(int col, int col_width)
{
	if (col_dragging_ != col || cols_min_widths_.size() <= (size_t)col)
		return;

	resizeBecauseOfDragging(col, col_width);

	allow_beyond_window_limits_when_dragging_ = false;
	col_dragging_ = -1;
}

void FluidColumnLayoutManager::onColDragging(int col, int col_width)
{
	if (col_dragging_ != col || cols_min_widths_.size() <= (size_t)col)
		return;

	resizeBecauseOfDragging(col, col_width);
}

void FluidColumnLayoutManager::resizeBecauseOfDragging(int col, int col_width)
{
	auto &col_min_width = cols_min_widths_[(size_t)col];

	if (col_width == col_min_width)
		return;

	col_min_width = col_width;
	if (persist_col_widths_)
		(*persist_col_widths_)[std::size_t(col)] = col_min_width;

	int sum_of_cols_widths = 0;
	for (auto c = 0; c < impl_->GetColumnCount(); ++c)
		sum_of_cols_widths += c == col ? col_width : impl_->GetColumnWidth(c);

	auto width_to_distribute = impl_->GetClientWidth() - sum_of_cols_widths;
	if (width_to_distribute <= 0 && allow_beyond_window_limits_when_dragging_)
		return;

	auto old_weights = weights_;
	std::fill(weights_.begin(), std::next(weights_.begin(), col), 0);

	DistributeWidth(width_to_distribute, col, col_width);

	weights_ = old_weights;
}

void FluidColumnLayoutManager::resize() {
	if (impl_->GetColumnCount() == 0)
		return;

	auto new_client_width = impl_->GetClientWidth();
	if (new_client_width == old_client_width_)
		return;

	int sum_of_cols_widths = 0;

	if (cols_min_widths_.size() != weights_.size()) {
		if (persist_col_min_widths_) {
			cols_min_widths_ = *persist_col_min_widths_;
			persist_col_widths_->resize(weights_.size(), -1);
		}

		cols_min_widths_.resize(weights_.size(), -1);

		for (std::size_t col = 0; col < cols_min_widths_.size(); ++col) {
			auto &col_min_width = cols_min_widths_[col];
			int col_width = col_min_width;

			if (col_width >= 0) {
				if (persist_col_widths_ && (*persist_col_widths_)[col] >= 0)
					col_width = (*persist_col_widths_)[col];

				impl_->SetColumnWidth(int(col), col_width);
			}
			else {
				col_width = col_min_width = impl_->GetColumnWidth(int(col));

				if (persist_col_widths_)
					(*persist_col_widths_)[col] = col_width;
			}

			sum_of_cols_widths += col_width;
		}
	}
	else {
		for (std::size_t col = 0; col < cols_min_widths_.size(); ++col) {
			auto col_width = impl_->GetColumnWidth(int(col));
			sum_of_cols_widths += col_width;
		}
	}

	auto width_to_distribute = new_client_width - sum_of_cols_widths;

	DistributeWidth(width_to_distribute);

	old_client_width_ = new_client_width;
}
