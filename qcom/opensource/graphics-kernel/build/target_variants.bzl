targets = [
    # keep sorted
    "canoe",
    "lahaina",
    "malabar",
    "monaco",
    "parrot",
    "sun",
    "vienna",
    "bengal",
    "chora",
    "shikra",
]

la_variants = [
    # keep sorted
    "consolidate",
    "perf",
]

gki_targets = [
    # keep sorted
    "anorak",
    "autoghgvm",
    "autogvm",
    "blair",
    "neo-la",
    "niobe",
    "pitti",
    "sdmsteppeauto",
    "seraph",
]

gki_variants = [
    # keep sorted
    "consolidate",
    "gki",
]

gki_perf_targets = [
    # keep sorted
    "gen3auto",
    "pineapple",
]

gki_perf_variants = [
    # keep sorted
    "consolidate",
    "gki",
    "perf"
]

le_targets = [
    # keep sorted
    "bengal-le",
    "vienna-le",
    "alor-le",
]

le_variants = [
    # keep sorted
    "debug-defconfig",
    "defconfig",
]

def get_all_variants():
    tv = [ (t, v) for t in targets for v in la_variants ]
    tv = tv + [ (t, v) for t in gki_targets for v in gki_variants ]
    tv = tv + [ (t, v) for t in gki_perf_targets for v in gki_perf_variants ]
    tv = tv + [ (t, v) for t in le_targets for v in le_variants ]

    return tv
