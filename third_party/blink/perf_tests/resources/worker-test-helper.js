// This file defines helper methods for running performance tests in workers.

(function () {
  class WorkerTestHelper {
    constructor() {
      this.callsPerIteration = 1;
    }

    // Measure the runs per second of test.run().
    // This method should be used together with
    // |PerfTestRunner.startMeasureValuesInWorker| in
    // src/third_party/blink/perf_tests/resources/runner.js.
    //
    // Arguments:
    // |test.run| is the function to test.
    // |test.setup| and |test.tearDown| are optional functions.
    // |test.iterationCount| defines count of iterations to run. Default value
    // is 5.
    //
    // Returns a promise that resolves to an object:
    // |result.error|: The error string or null if no error occurs.
    // |result.values|: An array of test result values.
    async measureRunsPerSecond(test) {
      this.test = test;
      const values = [];
      const iterationCount =
          this.test.iterationCount ? this.test.iterationCount : 5;

      try {
        if (this.test.setup)
          await this.test.setup();
        for (let i = 0; i < iterationCount; i++) {
          values.push(await this.measureRunsPerSecondOnce_());
        }
        if (this.test.tearDown)
          await this.test.tearDown();
      } catch (exception) {
        const error = "Got an exception while running test with name=" +
        exception.name + ", message=" + exception.message + "\n" +
        exception.stack;
        return { error: error, values: null };
      }
      return { error: null, values: values };
    }

    // This method is basically the same with measureRunsPerSecondOnce() in
    // src/third_party/blink/perf_tests/resources/runner.js
    async measureRunsPerSecondOnce_() {
      const timeToRun = 750;
      let totalTime = 0;
      let numberOfRuns = 0;

      while (totalTime < timeToRun) {
        totalTime += await this.callRunAndMeasureTime_();
        numberOfRuns += this.callsPerIteration;
        if (totalTime < 100)
          this.callsPerIteration = Math.max(10, 2 * this.callsPerIteration);
      }
      return numberOfRuns * 1000 / totalTime;
    };

    async callRunAndMeasureTime_() {
      var startTime = performance.now();
      for (let i = 0; i < this.callsPerIteration; i++) {
        await this.test.run();
      }
      return performance.now() - startTime;
    }
  }
  self.workerTestHelper = new WorkerTestHelper();
})();
