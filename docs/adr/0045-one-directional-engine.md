# Dominant axis and eight directions are one directional engine

Cited as D-F45. Was D-L28.

Eight-direction modes chose a wedge from whatever motion was banked, so a
slightly off-diagonal move alternated diagonal and straight taps, and dominant
axis chose its axis per report without smoothing. All directional modes now
run one engine: a smoothed heading, measured against each axis's own
threshold, picks a direction and holds it until the heading is clearly
elsewhere, and only progress along it counts. The axis policy says only which
directions exist; motion toward a missing one goes to the nearest one that
exists. Rules: [`pd-mode-domain-v1.md`](../architecture/pd-mode-domain-v1.md).
