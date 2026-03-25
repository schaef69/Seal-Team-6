# Seal-Team-6: March Madness ELO Predictions

An ELO-based bracket generator and game prediction tool for the NCAA March Madness tournament.

## Project Structure

- `data/`: Raw and processed tournament/game data.
  - `raw/games_2026.csv`: Mock regular season results for ELO training.
  - `raw/seeds_2026.csv`: Tournament seeds for bracket generation.
- `src/`: Core logic.
  - `models/elo.py`: ELO rating system with `fit` and `update` logic.
  - `bracket/generator.py`: Tournament simulation (Round 1, Elite Eight, Final Four, Championship).
- `main.py`: Main entry point to run the prediction pipeline.

## Getting Started

1. Install dependencies: `pip install -r requirements.txt`
2. Run the predictor: `python main.py`

## Current Status (March 2026)
- ELO Model: Implemented and trained on a subset of 2026 season data.
- Bracket Generator: Simplified 16-team simulation functional.
- Data: Using mock/scraped data for the 2026 season.
