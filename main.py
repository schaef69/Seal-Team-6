import pandas as pd
from src.models.elo import EloModel
from src.bracket.generator import BracketGenerator

def main():
    print("Welcome to Seal-Team-6 March Madness ELO Predictor!")
    
    # 1. Load Data
    try:
        games_df = pd.read_csv("data/raw/games_2026.csv")
        seeds_df = pd.read_csv("data/raw/seeds_2026.csv")
    except FileNotFoundError:
        print("Data files not found. Please ensure games_2026.csv and seeds_2026.csv exist in data/raw/")
        return

    # 2. Train ELO Model
    print("Training ELO model on regular season results...")
    model = EloModel(k_factor=30)
    model.fit(games_df)
    
    # Display top ELO ratings
    print("\nTop ELO Ratings:")
    sorted_ratings = sorted(model.ratings.items(), key=lambda x: x[1], reverse=True)
    for team, rating in sorted_ratings[:10]:
        print(f"{team}: {rating:.1f}")

    # 3. Generate Bracket
    print("\nSimulating the 2026 Tournament Bracket (Simplified)...")
    generator = BracketGenerator(model)
    bracket_results = generator.generate_bracket(seeds_df)
    
    # 4. Display Results
    print("\n--- Bracket Prediction ---")
    print(f"Round 1 Winners: {', '.join(bracket_results['Round 1 Winners'])}")
    print(f"Elite Eight: {', '.join(bracket_results['Elite Eight Winners'])}")
    print(f"Final Four: {', '.join(bracket_results['Finalists'])}")
    print(f"NATIONAL CHAMPION: {bracket_results['Champion']}")

if __name__ == "__main__":
    main()
