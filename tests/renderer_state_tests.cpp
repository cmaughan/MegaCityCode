#include "support/test_support.h"

#include "renderer_state.h"

#include <vector>

using namespace spectre;
using namespace spectre::tests;

void run_renderer_state_tests()
{
    run_test("renderer state projects cell updates into gpu cells", []() {
        RendererState state;
        state.set_grid_size(2, 1, 1);
        state.set_cell_size(10, 20);
        state.set_ascender(16);

        CellUpdate update;
        update.col = 1;
        update.row = 0;
        update.bg = { 0.1f, 0.2f, 0.3f, 1.0f };
        update.fg = { 0.9f, 0.8f, 0.7f, 1.0f };
        update.sp = { 0.5f, 0.4f, 0.3f, 1.0f };
        update.glyph = { 0.1f, 0.2f, 0.3f, 0.4f, 2, 3, 6, 7 };
        update.style_flags = 5;
        state.update_cells({ &update, 1 });

        std::vector<GpuCell> gpu(state.total_cells() + RendererState::OVERLAY_CELL_CAPACITY + 1);
        state.copy_to(gpu.data());

        expect_eq(static_cast<int>(state.buffer_size_bytes()),
            static_cast<int>((state.total_cells() + RendererState::OVERLAY_CELL_CAPACITY + 1) * sizeof(GpuCell)),
            "buffer includes overlay region and cursor slot");
        expect_eq(static_cast<int>(gpu[1].pos_x), 11, "cell x position uses cell width and padding");
        expect_eq(static_cast<int>(gpu[1].pos_y), 1, "cell y position uses padding");
        expect_eq(gpu[1].bg_b, 0.3f, "background color is copied");
        expect_eq(gpu[1].fg_r, 0.9f, "foreground color is copied");
        expect_eq(gpu[1].sp_r, 0.5f, "special color is copied");
        expect_eq(gpu[1].glyph_size_x, 6.0f, "glyph width is copied");
        expect_eq(gpu[1].glyph_offset_y, 7.0f, "glyph y offset uses ascender and cell height");
        expect_eq(gpu[1].style_flags, static_cast<uint32_t>(5), "style flags are copied");
    });

    run_test("renderer state restores block cursor overlays cleanly", []() {
        RendererState state;
        state.set_grid_size(1, 1, 1);

        CellUpdate update;
        update.col = 0;
        update.row = 0;
        update.bg = { 0.2f, 0.3f, 0.4f, 1.0f };
        update.fg = { 0.8f, 0.7f, 0.6f, 1.0f };
        state.update_cells({ &update, 1 });

        CursorStyle cursor;
        cursor.shape = CursorShape::Block;
        cursor.use_explicit_colors = true;
        cursor.fg = { 1.0f, 1.0f, 1.0f, 1.0f };
        cursor.bg = { 0.0f, 0.0f, 0.0f, 1.0f };

        state.set_cursor(0, 0, cursor);
        state.apply_cursor();

        std::vector<GpuCell> gpu(state.total_cells() + RendererState::OVERLAY_CELL_CAPACITY + 1);
        state.copy_to(gpu.data());
        expect_eq(gpu[0].bg_r, 0.0f, "block cursor overrides background");
        expect_eq(gpu[0].fg_r, 1.0f, "block cursor overrides foreground");

        state.restore_cursor();
        state.copy_to(gpu.data());
        expect_eq(gpu[0].bg_r, 0.2f, "restoring cursor restores background");
        expect_eq(gpu[0].fg_r, 0.8f, "restoring cursor restores foreground");
    });

    run_test("renderer state appends overlay geometry for line cursors", []() {
        RendererState state;
        state.set_grid_size(1, 1, 1);
        state.set_cell_size(10, 20);

        CellUpdate update;
        update.col = 0;
        update.row = 0;
        state.update_cells({ &update, 1 });

        CursorStyle cursor;
        cursor.shape = CursorShape::Vertical;
        cursor.bg = { 1.0f, 0.0f, 0.0f, 1.0f };
        cursor.cell_percentage = 30;

        state.set_cursor(0, 0, cursor);
        state.apply_cursor();

        std::vector<GpuCell> gpu(state.total_cells() + RendererState::OVERLAY_CELL_CAPACITY + 1);
        state.copy_to(gpu.data());

        expect_eq(state.bg_instances(), 2, "line cursor adds one overlay background instance");
        expect_eq(gpu[state.total_cells()].bg_r, 1.0f, "overlay cell carries cursor background");
        expect_eq(gpu[state.total_cells()].size_x, 3.0f, "overlay width uses cell percentage");
        expect_eq(gpu[state.total_cells()].size_y, 20.0f, "overlay height spans the cell");
    });

    run_test("renderer state keeps debug overlay cells separate from the grid", []() {
        RendererState state;
        state.set_grid_size(2, 1, 1);
        state.set_cell_size(10, 20);
        state.set_ascender(16);
        state.clear_dirty();

        CellUpdate overlay;
        overlay.col = 1;
        overlay.row = 0;
        overlay.bg = { 0.0f, 0.0f, 0.0f, 0.8f };
        overlay.fg = { 0.1f, 0.8f, 1.0f, 1.0f };
        overlay.sp = overlay.fg;
        overlay.glyph = { 0.1f, 0.2f, 0.3f, 0.4f, 1, 2, 7, 8 };
        state.set_overlay_cells({ &overlay, 1 });

        std::vector<GpuCell> gpu(state.total_cells() + RendererState::OVERLAY_CELL_CAPACITY + 1);
        state.copy_to(gpu.data());

        expect_eq(state.bg_instances(), 3, "debug overlay contributes a background instance");
        expect_eq(state.fg_instances(), 3, "debug overlay contributes a foreground instance");
        expect(state.overlay_region_dirty(), "setting the debug overlay dirties the overlay region");
        expect_eq(gpu[state.total_cells()].pos_x, 11.0f, "overlay cell position uses grid coordinates");
        expect_eq(gpu[state.total_cells()].fg_g, 0.8f, "overlay cell foreground is copied");
    });

    run_test("renderer state tracks dirty ranges for incremental uploads", []() {
        RendererState state;
        state.set_grid_size(3, 1, 1);
        expect(state.has_dirty_cells(), "initial layout dirties the cell buffer");
        expect_eq(static_cast<int>(state.dirty_cell_offset_bytes()), 0, "initial dirty range starts at zero");
        expect_eq(static_cast<int>(state.dirty_cell_size_bytes()), static_cast<int>(3 * sizeof(GpuCell)), "initial dirty range spans the grid");
        expect(state.overlay_region_dirty(), "initial layout dirties the overlay region too");

        state.clear_dirty();

        CellUpdate update_a;
        update_a.col = 0;
        update_a.row = 0;
        CellUpdate update_b;
        update_b.col = 2;
        update_b.row = 0;
        CellUpdate updates[] = { update_a, update_b };
        state.update_cells(updates);

        expect(state.has_dirty_cells(), "cell updates produce a dirty range");
        expect_eq(static_cast<int>(state.dirty_cell_offset_bytes()), 0, "dirty range starts at the first updated cell");
        expect_eq(static_cast<int>(state.dirty_cell_size_bytes()), static_cast<int>(3 * sizeof(GpuCell)), "dirty range expands to cover the touched span");
        expect(!state.overlay_region_dirty(), "plain cell updates do not dirty the overlay region");

        CursorStyle cursor;
        cursor.shape = CursorShape::Vertical;
        cursor.bg = { 1.0f, 0.0f, 0.0f, 1.0f };
        cursor.cell_percentage = 25;
        state.set_cursor(1, 0, cursor);
        state.apply_cursor();

        expect(state.overlay_region_dirty(), "overlay cursor dirties the overlay region");
    });
}
