"""Approves an evaluated model for the game, or refuses it with the reasons.

Only a model whose evaluation report says "accepted" is copied to projects/reference/assets/policies/, with the
report's summary stored inside it. A refused model never reaches the game.

    python approve.py --model m.json --report report.json --out ../../projects/reference/assets/policies/bandit_leader.policy.json
"""
import argparse
import json
import sys


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--model", required=True)
    ap.add_argument("--report", required=True)
    ap.add_argument("--out", required=True)
    args = ap.parse_args()

    model, report = json.load(open(args.model)), json.load(open(args.report))
    if report.get("model") != args.model:
        sys.exit(f"REFUSED: the report is for {report.get('model')}, not {args.model}")
    acceptance = report["acceptance"]
    if not acceptance["accepted"]:
        failed = [name for name, ok in acceptance["checks"].items() if not ok]
        sys.exit("REFUSED: failed checks: " + ", ".join(failed) + " (see the report)")

    # Keep the evidence with the model: what was measured, against which thresholds.
    learned = report["learned"]
    model["evaluation"] = {
        "accepted": True, "episodes": report["episodes"], "heldOutSeedsFrom": report["heldOutSeedsFrom"],
        "winRate": learned["winRate"], "conventionalWinRate": report["conventional"]["winRate"],
        "randomWinRate": report["random"]["winRate"],
        "preferenceGap": learned["probes"]["gap"],
        "controlPreferenceGap": report.get("control", {}).get("probes", {}).get("gap"),
        "healsWhenNearlyDeadVsHealthyOpponent": learned["probes"]["healsWhenNearlyDeadVsHealthyOpponent"],
        "attacksWhenBothNearlyDead": learned["probes"]["attacksWhenBothNearlyDead"],
        "parityMaxError": report["parityMaxError"], "thresholds": acceptance["thresholds"],
    }
    with open(args.out, "w") as f:
        json.dump(model, f, indent=1)
    print(f"APPROVED: wrote {args.out}")


if __name__ == "__main__":
    main()
