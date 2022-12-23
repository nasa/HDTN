class RateCalculator {
    constructor() {
        this.currentTotalBytes = 0;
        this.lastTotalBytes = 0;
        this.startingDataBytes = 0;
        this.currentTime = 0;
        this.prevTime = 0;
        this.startTime = new Date().getTime()*1000;
        this.count = 0;
    }

    get averageMbpsRate() {
        return (8.0 * (this.currentTotalBytes - this.startingDataBytes))/(this.currentTime - this.startTime);
    }

    get currentMbpsRate() {
        let duration = this.currentTime - this.prevTime;
        return (8.0 * (this.currentTotalBytes - this.lastTotalBytes))/duration;
    }

    appendVal(totalBytes) {
        this.prevTime = this.currentTime;
        this.lastTotalBytes = this.currentTotalBytes

        this.currentTime = new Date().getTime()*1000;
        this.currentTotalBytes = totalBytes;
        this.count++;
        if (this.count == 1) {
            this.startingDataBytes = totalBytes;
        }
    }
}
