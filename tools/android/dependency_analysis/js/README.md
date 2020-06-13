# Chrome Android Dependency Analysis Visualization
## Development Setup
### Shell variables

This setup assumes Chromium is in a `cr` directory (`~/cr/src/...`). To make setup easier, you can modify and export the following variables:
```
export DEP_ANALYSIS_DIR=~/cr/src/tools/android/dependency_analysis
export DEP_ANALYSIS_JAR=~/cr/src/out/Default/obj/chrome/android/chrome_java__process_prebuilt.desugar.jar
```

### Generate JSON

See `../README.md` for instructions on using `generate_json_dependency_graph.py`, then generate a graph file in this directory (`js/json_graph.txt`) with that exact name:

```
cd $DEP_ANALYSIS_DIR
./generate_json_dependency_graph.py --target $DEP_ANALYSIS_JAR --output js/json_graph.txt
```
**The following instructions assume you are in the `dependency_analysis/js` = `$DEP_ANALYSIS_DIR/js` directory.**

### Install dependencies
You will need to install `npm` if it is not already installed (check with `npm -v`), either [from the site](https://www.npmjs.com/get-npm) or via [nvm](https://github.com/nvm-sh/nvm#about) (Node Version Manager).

To install dependencies:

```
npm install
```

### (TEMP) Run visualization

To run the (highly temporary) Python server to serve the JSON at `localhost:8888/json_graph.txt` :
```
python3 -m http.server 8888
```
The visualization will make requests to this server for the JSON graph on load.

**To view the visualization, open `localhost:8888/index.html`.**

### Miscellaneous
To run [ESLint](https://eslint.org/) on the JS (and fix fixable errors) using [npx](https://www.npmjs.com/package/npx) (bundled with npm):
```
npx eslint --fix *.js
```
