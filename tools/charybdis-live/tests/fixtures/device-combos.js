"use strict";
const fs = require("node:fs");
const path = require("node:path");
const {fnv1a32} = require("../../core/schema/profile-blob-v1");

// The firmware's own byte fixtures: version 2 by default, version 1 for the
// older firmware the app still reads.
function fixturePages(version = 2) {
    return fs.readFileSync(path.resolve(__dirname, `../../../../tests/fixtures/combo_readback_v${version}.fixture`), "utf8")
        .trim().split("\n").map(line => Buffer.from(line.split(" ")[1], "hex"));
}
// Version 2's digest covers every metadata byte but the digest itself.
function rehash(pages) {
    const covered = pages[0][0] === 2 ? [pages[0].subarray(0, 14), pages[0].subarray(18)] : [pages[0].subarray(0, 14)];
    pages[0].writeUInt32LE(fnv1a32(Buffer.concat([...covered, ...pages.slice(1)])), 14);
    return pages;
}
function responseFor(request, pages) {
    const response = Buffer.alloc(32);
    request.copy(response, 0, 0, 5);
    response[6] = 25;
    pages[request[4]].copy(response, 7);
    return response;
}
module.exports = {fixturePages, rehash, responseFor};
