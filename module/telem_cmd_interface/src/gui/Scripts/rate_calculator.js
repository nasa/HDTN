class RateCalculator {
    constructor() {
        this.currentTotalBytes = 0;
        this.lastTotalBytes = 0;
        this.startingDataBytes = 0;
        this.currentTimeMs = 0;
        this.prevTimeMs = 0;
        this.startTimeMs = null;
        this.count = 0;
    }

    get averageMbpsRate() {
        const durationSeconds = (this.currentTimeMs - this.startTimeMs) / 1000.0;
        return 1e-6 * (8.0 * (this.currentTotalBytes - this.startingDataBytes)) / durationSeconds;
    }

    get currentMbpsRate() {
        const durationSeconds = (this.currentTimeMs - this.prevTimeMs) / 1000.0;
        return 1e-6 * (8.0 * (this.currentTotalBytes - this.lastTotalBytes)) / durationSeconds;
    }

    appendVal(totalBytes, timestampMilliseconds) {
        if(this.startTimeMs == null) {
            this.startTimeMs = timestampMilliseconds;
        }
        this.prevTimeMs = this.currentTimeMs;
        this.lastTotalBytes = this.currentTotalBytes

        this.currentTimeMs = timestampMilliseconds;
        this.currentTotalBytes = totalBytes;
        this.count++;
        if (this.count == 1) {
            this.startingDataBytes = totalBytes;
        }
    }
}
