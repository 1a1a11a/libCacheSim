use std::path::PathBuf;

use crate::SimulationResult;
use gnuplot::{
    AutoOption::Fix,
    AxesCommon, Figure,
    PlotOption::{Caption, LineWidth},
};
use tracing::{debug, info};

// Draw lines representing miss ratio curves based on simulation results.
// Parameters:
// - results: A slice of SimulationResult, each containing data points to be plotted.
// - path: PathBuf where the generated PNG image will be saved.
pub fn draw_lines(results: &[SimulationResult], path: PathBuf) {
    let mut fg = Figure::new();

    // Define the dimensions for the output image.
    let width = 1920;
    let height = 1080;

    // Set the title of the plot.
    fg.set_title("Miss ratio curve");
    let axes = fg.axes2d();

    // Enable grid lines on both x and y axes.
    axes.set_x_grid(true)
        .set_y_grid(true)
        // Set the range for the y-axis from 0 to 1 to standardize the scale.
        .set_y_range(Fix(0.0), Fix(1.0));

    for result in results {
        axes.set_x_label("Cache size", &[])
            .set_y_label("Miss ratio", &[])
            // Plot lines for the current result, using cache sizes as x values and miss ratios as y values.
            // Each line is labeled with the result's label.
            .lines(
                result.points.iter().map(|(x, _)| *x),
                result.points.iter().map(|(_, y)| *y),
                &[Caption(result.label.as_str()), LineWidth(2.0)],
            );
    }

    // Save the figure to a PNG file at the specified path.
    match fg.save_to_png(path.clone(), width, height) {
        Ok(_) => info!("Saved plot to {:?}", path),
        Err(e) => panic!("Failed to save plot: {:?}", e),
    }
}
