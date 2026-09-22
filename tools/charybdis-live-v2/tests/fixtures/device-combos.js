"use strict";
const fs = require("node:fs");
const path = require("node:path");
const {fnv1a32} = require("../../core/schema/profile-blob-v1");

function fixturePages() {
    return fs.readFileSync(path.resolve(__dirname, "../../../../tests/fixtures/combo_readback_v1.fixture"), "utf8")
        .trim().split("\n").map(line => Buffer.from(line.split(" ")[1], "hex"));
}
function rehash(pages) {
    pages[0].writeUInt32LE(fnv1a32(Buffer.concat([pages[0].subarray(0, 14), ...pages.slice(1)])), 14);
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
