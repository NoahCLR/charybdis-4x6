"use strict";

const {changedRanges} = require("../protocol/via-storage-v1");
const {LOGICAL_VIA_STAGE_V1, LOGICAL_VIA_STATE, buildLogicalViaAbortRequest, buildLogicalViaBeginRequest, buildLogicalViaChunkRequest, buildLogicalViaVerifyRequest, readLogicalViaStatus, sendLogicalViaMutation} = require("../protocol/logical-via-stage-v1");

const REGIONS = Object.freeze({CONFIG: 1, KEYMAP: 2, MACRO: 4});
const delay = ms => new Promise(resolve => setTimeout(resolve, ms));

class LogicalViaStageCoordinator {
    constructor(connection, {requestIds, pollMs = 20, timeoutMs = 90000, onProgress = () => {}} = {}) {
        if (!connection || typeof connection.request !== "function" || !requestIds || typeof requestIds.next !== "function") throw new TypeError("Logical VIA staging requires a connection and request ids.");
        this.connection = connection;
        this.requestIds = requestIds;
        this.pollMs = pollMs;
        this.timeoutMs = timeoutMs;
        this.onProgress = onProgress;
    }

    async waitFor(transactionId, operationSequence, acceptedStates) {
        const deadline = Date.now() + this.timeoutMs;
        do {
            const status = await readLogicalViaStatus(this.connection, {nextRequestId: () => this.requestIds.next()});
            if (status.state === LOGICAL_VIA_STATE.ERROR) throw Object.assign(new Error(`Peer VIA staging failed with status ${status.lastStatus}.`), {code: "LOGICAL_VIA_STAGE_FAILED", status});
            if (status.transactionId === transactionId && !status.pending && status.operationSequence > operationSequence && acceptedStates.includes(status.state)) return status;
            await delay(this.pollMs);
        } while (Date.now() < deadline);
        throw Object.assign(new Error("Timed out while staging the VIA profile on the other half."), {code: "LOGICAL_VIA_STAGE_TIMEOUT"});
    }

    async operation(request, transactionId, acceptedStates) {
        const before = await readLogicalViaStatus(this.connection, {nextRequestId: () => this.requestIds.next()});
        await sendLogicalViaMutation(this.connection, request);
        return this.waitFor(transactionId, before.operationSequence, acceptedStates);
    }

    async stage({transactionId, generation, digest, target, current}) {
        await this.operation(buildLogicalViaBeginRequest(transactionId, generation, digest), transactionId, [LOGICAL_VIA_STATE.STAGING]);
        const regions = [
            {id: REGIONS.CONFIG, bytes: Buffer.from([1, 0]), ranges: [{offset: 0, bytes: Buffer.from([1, 0])}]},
            {id: REGIONS.KEYMAP, bytes: target.layout, ranges: changedRanges(current.layout, target.layout)},
            {id: REGIONS.MACRO, bytes: target.macros, ranges: changedRanges(current.macros, target.macros)},
        ];
        const total = regions.reduce((sum, region) => sum + region.ranges.reduce((subtotal, range) => subtotal + range.bytes.length, 0), 0);
        let completed = 0;
        for (const region of regions) {
            for (const range of region.ranges) {
                for (let inner = 0; inner < range.bytes.length; inner += LOGICAL_VIA_STAGE_V1.CHUNK_MAX) {
                    const bytes = range.bytes.subarray(inner, inner + LOGICAL_VIA_STAGE_V1.CHUNK_MAX);
                    const request = buildLogicalViaChunkRequest(transactionId, {region: region.id, offset: range.offset + inner, regionLength: region.bytes.length, bytes, generation, digest});
                    await this.operation(request, transactionId, [LOGICAL_VIA_STATE.STAGING]);
                    completed += bytes.length;
                    this.onProgress({phase: "staging-via", completed, total});
                }
            }
        }
        return this.operation(buildLogicalViaVerifyRequest(transactionId, generation, digest), transactionId, [LOGICAL_VIA_STATE.STAGED]);
    }

    async waitUntilAccepted({transactionId, generation, digest}) {
        const deadline = Date.now() + this.timeoutMs;
        do {
            const status = await readLogicalViaStatus(this.connection, {nextRequestId: () => this.requestIds.next()});
            const exact = status.transactionId === transactionId && status.generation === generation && status.digest === digest;
            if (status.state === LOGICAL_VIA_STATE.ERROR) throw Object.assign(new Error(`Peer VIA acceptance failed with status ${status.lastStatus}.`), {code: "LOGICAL_VIA_ACCEPT_FAILED", status});
            // IDLE with the exact retained identity means firmware already
            // released the normal-reconciliation fence after local roll-forward.
            if (exact && !status.pending && [LOGICAL_VIA_STATE.ACCEPTED, LOGICAL_VIA_STATE.IDLE].includes(status.state)) return status;
            await delay(this.pollMs);
        } while (Date.now() < deadline);
        throw Object.assign(new Error("Timed out waiting for the staged VIA profile to become the recovery authority."), {code: "LOGICAL_VIA_ACCEPT_TIMEOUT"});
    }

    async abort({transactionId, generation, digest}) {
        return this.operation(buildLogicalViaAbortRequest(transactionId, generation, digest), transactionId, [LOGICAL_VIA_STATE.ABORTED]);
    }
}

module.exports = {LogicalViaStageCoordinator, REGIONS};
