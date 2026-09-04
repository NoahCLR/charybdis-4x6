import {element, facts, hex, panel} from "../dom.js";

const ACTIVE_KIND = {0: "compiled defaults", 1: "validated profile"};

export function renderStatus(snapshot) {
    const status = snapshot.status;
    if (!status) {
        return panel("Committed profile", [element("p", {class: "muted"}, "No status page read yet.")]);
    }

    const generationsAgree =
        status.activeGeneration === status.committedGeneration &&
        status.committedGeneration === status.peerGeneration;

    return panel("Committed profile", [
        element(
            "p",
            {class: generationsAgree ? "chip chip--ok" : "chip chip--warn"},
            generationsAgree ? "Both halves report the same generation" : "Halves disagree or have not converged"
        ),
        facts([
            ["Active kind", ACTIVE_KIND[status.activeKind] ?? status.activeKind],
            ["Active generation", status.activeGeneration],
            ["Active digest", hex(status.activeDigest)],
            ["Committed generation", status.committedGeneration],
            ["Committed digest", hex(status.committedDigest)],
            ["Committed origin half", status.committedOriginHalf],
            ["Peer generation", status.peerGeneration],
            ["Peer origin half", status.peerOriginHalf],
            ["Pending digest", hex(status.pendingDigest)],
            ["Source digest", hex(status.sourceDigest)],
            ["Validation state", status.validationState],
            ["Conflicts", status.conflictCount],
            ["Last error", status.lastError],
            ["State flags", hex(status.stateFlags, 4)],
        ]),
        element(
            "p",
            {class: "muted"},
            "This is capability and status reporting. Reading the committed payload back is the next slice."
        ),
    ]);
}
