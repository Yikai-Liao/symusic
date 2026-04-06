# src/my_sci_package/cli.py
import click
import numpy as np
from pathlib import Path

@click.group()
@click.version_option()
def cli():
    """Scientific analysis CLI tool."""
    pass

@cli.command()
@click.argument("input_file", type=click.Path(exists=True))
@click.option("--output", "-o", type=click.Path(), help="Output file path")
@click.option("--threshold", "-t", type=float, default=0.5, help="Analysis threshold")
def analyze(input_file: str, output: str, threshold: float):
    """Analyze scientific data from input file."""
    # Load and analyze data
    data = np.loadtxt(input_file)
    result = np.mean(data[data > threshold])

    click.echo(f"Analysis complete: mean = {result:.4f}")

    if output:
        np.savetxt(output, [result])
        click.echo(f"Results saved to {output}")

@cli.command()
@click.argument("input_file", type=click.Path(exists=True))
@click.option("--format", type=click.Choice(["png", "pdf", "svg"]), default="png")
def plot(input_file: str, format: str):
    """Generate plots from data."""
    import matplotlib.pyplot as plt

    data = np.loadtxt(input_file)
    plt.plot(data)
    output_file = f"plot.{format}"
    plt.savefig(output_file)
    click.echo(f"Plot saved to {output_file}")

def main():
    """Entry point for CLI."""
    cli()

if __name__ == "__main__":
    main()
