/**
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */


const renderSet = (inMandelbrotSet) => {
  // Create Canvas
  const myCanvas = document.createElement('canvas');
  myCanvas.width = 1920;
  myCanvas.height = 1080;
  document.body.appendChild(myCanvas);
  const ctx = myCanvas.getContext('2d');

  // Set appearance settings
  const magnificationFactor = 8500;
  const panX = 0.8;
  const panY = 0.4;
  const data = [];
  const t0 = performance.now();
  for (let x = 0; x < myCanvas.width; x++) {
    for (let y = 0; y < myCanvas.height; y++) {
      const xp = x / magnificationFactor - panX;
      const yp = y / magnificationFactor - panY;
      const belongsToSet = inMandelbrotSet(xp, yp);
      data.push(belongsToSet);
    }
  }
  const t1 = performance.now();
  console.log('calculated set data in ' + (t1 -t0) / 1000 + ' sec.');
  const t2 = performance.now();
  for (let x = 0; x < myCanvas.width; x++) {
    for (let y = 0; y < myCanvas.height; y++) {
      const belongsToSet=data[x * myCanvas.height + y];
      if (belongsToSet === 0) {
        ctx.fillStyle = '#000';
        // Draw a black pixel
        ctx.fillRect(x,y, 1,1);
      } else {
        ctx.fillStyle = `hsl(0, 100%, ${belongsToSet}%)`;
        // Draw a colorful pixel
        ctx.fillRect(x,y, 1,1);
      }
    }
  }
  const t3 = performance.now();
  console.log('rendered in ' + (t3 - t2) / 1000 + ' sec');

}

const loadAndRender = () => {
  console.log('in loadAndRender')
  loadWasmApp('mandelbrot.wasm')
  .then(ctx => {
    console.log('load Wasm complete: ', ctx)

    const genMandelbrotSet = ctx.instance.exports['sk.genMandelbrotSet']

/*
    const t0 = performance.now();
    const width = 1920;
    const height = 1080;
    const panX = 0.8;
    const panY = 0.4;
    const magFactor = 8500;
    vec = genMandelbrotSet(width, height, panX, panY, magFactor);
    const t1 = performance.now();
    console.log('generated full set data in ' + (t1 - t0) / 1000 + ' sec')

    const data = wasmUtils.getRawData(ctx.env, vec, 80);
    console.log('raw set data: ', data);
*/

    const inMandelbrot = ctx.instance.exports['sk.inMandelbrotTail']

    const xp = -0.8
    const yp = -0.4
    const cover = inMandelbrot(xp, yp)
    console.log(xp, yp, ' --> ', cover)

    renderSet(inMandelbrot)
  })
  .catch(err => {
    console.log(err)
  })
}
