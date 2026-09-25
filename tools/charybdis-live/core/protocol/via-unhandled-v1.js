"use strict";

// QMK's answer to a command nothing on the keyboard handled.
//
// quantum/via.c always replies: when no handler takes a report — a candidate
// frame while the profile owner is not ready, a GET page this firmware does
// not have — byte 0 becomes id_unhandled (0xFF) and bytes 1–31 come back
// exactly as sent. That echo is a definite answer: the keyboard received the
// request and did nothing with it. Treating it as silence would wait out the
// timeout, drop the connection, and report an unknown outcome for a request
// the keyboard plainly refused.
//
// Only the full echo counts, so a stray 0xFF report — another VIA client's
// reply, or one for a different request — is never taken for this one.

const {normalizeRawHidReport} = require("../transport/device-adapter");

const VIA_UNHANDLED = 0xff;

function isUnhandledEcho(response, request) {
    const actual = normalizeRawHidReport(response, "Raw HID response");
    const sent = normalizeRawHidReport(request, "Raw HID request");
    return actual[0] === VIA_UNHANDLED && sent[0] !== VIA_UNHANDLED && actual.subarray(1).equals(sent.subarray(1));
}

// A matcher for `request` that also accepts the keyboard's unhandled echo of
// it. The request is bound here, so the matcher works whether or not the
// caller passes it back.
const orUnhandled = (matcher, request) => (response, sent = request) =>
    isUnhandledEcho(response, sent) || Boolean(matcher(response, sent));

function unhandledError(what) {
    return Object.assign(new Error(`The keyboard did not handle ${what}. It may not support it, or may not be ready for it; read the keyboard again.`), {
        code: "UNHANDLED",
        unhandled: true,
    });
}

// Sends a request whose reply is matched by `options.matchResponse`, and turns
// the keyboard's unhandled echo into an UNHANDLED error. The connection stays
// open: the keyboard answered.
async function requestHandled(connection, request, options, what) {
    const response = await connection.request(request, {...options, matchResponse: orUnhandled(options.matchResponse, request)});
    if (isUnhandledEcho(response, request)) throw unhandledError(what);
    return response;
}

module.exports = {VIA_UNHANDLED, isUnhandledEcho, orUnhandled, requestHandled, unhandledError};
