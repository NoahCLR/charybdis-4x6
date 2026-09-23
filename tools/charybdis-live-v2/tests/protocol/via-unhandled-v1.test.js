"use strict";
const assert = require("node:assert/strict");
const test = require("node:test");
const {VIA_UNHANDLED, isUnhandledEcho, orUnhandled, requestHandled} = require("../../core/protocol/via-unhandled-v1");
const {readProfilePages} = require("../../core/protocol/profile-wire-v1");

const request = () => Buffer.from([0x08, 0x00, 0x0c, 7, 2, ...Array(27).fill(0)]);
// What QMK sends when nothing handles a report (quantum/via.c).
const echo = (sent) => Object.assign(Buffer.from(sent), {0: VIA_UNHANDLED});

test("only the keyboard's echo of this request counts as unhandled", () => {
    const sent = request();
    assert.equal(isUnhandledEcho(echo(sent), sent), true);
    assert.equal(isUnhandledEcho(Buffer.alloc(32, 0xff), sent), false, "a stray 0xFF report");
    const other = echo(sent); other[3] = 8;
    assert.equal(isUnhandledEcho(other, sent), false, "the echo of a different request");
    assert.equal(isUnhandledEcho(sent, sent), false, "a normal reply");
    const matcher = orUnhandled(() => false, sent);
    assert.equal(matcher(echo(sent)), true, "bound to its request, so a one-argument call still works");
});

test("an unhandled request is a definite UNHANDLED error, answered at once", async () => {
    const sent = request();
    let requests = 0;
    const connection = {async request(report, options) {
        requests++;
        const reply = echo(report);
        assert.equal(options.matchResponse(reply, report), true);
        return reply;
    }};
    await assert.rejects(requestHandled(connection, sent, {matchResponse: () => false}, "the test page"),
        (error) => error.code === "UNHANDLED" && error.unhandled === true && /test page/.test(error.message));
    // Profile Wire page reads used to wait out the timeout here and drop the link.
    await assert.rejects(readProfilePages(connection, 0x04, 1, {requestId: 3}), {code: "UNHANDLED"});
    assert.equal(requests, 2);
});
