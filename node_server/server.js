const tf = require('@tensorflow/tfjs-node-gpu');
const bodyPix = require('@tensorflow-models/body-pix');
const net = require('net');
const fs = require('fs');
const process = require('process');
const cv = require('opencv');

const CONFIG = require('./config');

const LOCALHOST = "localhost";
const outputFile = `${process.env.TMPDIR || "/tmp"}/.segmentation.port`;

const REQUEST_HEADER = Buffer.from([0xee, 0x61, 0xbe, 0xc4, 0x38, 0xd2, 0x56, 0xa9]);
const RESPONSE_HEADER = Buffer.from([0x50, 0x77, 0x3d, 0xda, 0xc8, 0x7d, 0x5d, 0x97]);
let NUM_FRAMES = 0;


const socketDataPromise = (socket) => {
    return new Promise((resolve, reject) => {
        socket.on('data', (chunk) => resolve(chunk));
    });
}

const writePromise = (socket, data) => {
    return new Promise((resolve, reject) => {
        socket.write(data, null, () => {
            resolve();
        });
    });
}


const timer = (name, func) => {
    const hrstart = process.hrtime();
    const result = func();
    const hrend = process.hrtime(hrstart);
    if (CONFIG.debugTimings) {
        console.info(` !! Timing: ${name}: ${hrend[0]}s ${hrstart[1]}ns`);
    }
    return result;
};

const timeit = (name, hrstart, debug) => {
    const hrend = process.hrtime(hrstart);
    if (debug || CONFIG.debugTimings) {
        console.info(` !! Timing: ${name} ${hrend[0]}s ${hrend[1]/1000}us`);
    }
    return process.hrtime();
};


(function () {
    function getRandomPort() {
        return Math.floor(Math.random() * (32767 - 1024) + 1024);
    }

    function getIntBuffer(value) {
        const buf = new Buffer(4);
        buf.writeInt32LE(value);
        return buf;
    }

    async function handleConnection(nn, socket) {
        console.log("!! Got new connection");
        let running = true;
        socket.setTimeout(1);
        socket.on('end', () => {
            running = false;
        });
        socket.on('error', () => {
            socket.destroy();
            running = false;
        });
        let holderHeight = 0;
        let holderWidth = 0;
        let cvImageHolder = null;
        let maskImageHolder = null;
        while (running) {
            NUM_FRAMES++;
            if (NUM_FRAMES > 750) {
                process.exit(0);
            }
            const chunks = [];
            let offset = 0;
            let currentTotalSize = 0;

            let start = process.hrtime();
            const requestStart = process.hrtime();

            while (currentTotalSize < 12) {
                const buf = await socketDataPromise(socket);
                chunks.push(buf);
                currentTotalSize += buf.length;
            }
            const currentBuffer = (chunks.length === 1) ? chunks[0] : Buffer.concat(chunks);
            const requestHeader = currentBuffer.subarray(0, 8);

            if (!requestHeader.equals(REQUEST_HEADER)) {
                socket.destroy();
            }
            
            offset += 8;
            const requestSize = currentBuffer.readInt32LE(offset);
            offset += 4;

            while (currentTotalSize < requestSize) {
                const buf = await socketDataPromise(socket);
                chunks.push(buf);
                currentTotalSize += buf.length;
            }

            const requestBuffer = Buffer.concat(chunks);

            start = timeit("loaded buffer", start);
        
            const segmentationThreshold = requestBuffer.readFloatLE(offset);
            offset += 4;
            const height = requestBuffer.readInt16LE(offset);
            offset += 2;
            const width = requestBuffer.readInt16LE(offset);
            offset += 2;
            const blur = requestBuffer.readInt16LE(offset);
            offset += 2;
            const growshrink = requestBuffer.readInt16LE(offset);
            offset += 2;

            if (holderHeight != height || holderWidth != width || cvImageHolder === null) {
                if (cvImageHolder !== null) {
                    cvImageHolder.release();
                    maskImageHolder.release();
                }
                cvImageHolder = new cv.Matrix(height, width, cv.Constants.CV_8UC3);
                maskImageHolder = new cv.Matrix(height, width, cv.Constants.CV_8UC1);
                holderHeight = height;
                holderWidth = width;
            }
            cvImageHolder.put(requestBuffer.subarray(offset));
            start = timeit("added data to image holder", start);

            const image = tf.node.decodeImage(cvImageHolder.toBuffer({ext: ".bmp"}));
            start = timeit("tf decoded image", start);
            if (image === null) {
                await writePromise(socket, RESPONSE_HEADER);
                await writePromise(socket, getIntBuffer(-1));
            }
            const options = CONFIG.segmentationOptions;
            options.segmentationThreshold = segmentationThreshold;
            const segmentation = await nn.segmentPerson(image, options);
            tf.dispose(image);
            maskImageHolder.put(Buffer.from(segmentation.data));

            start = timeit("finished segmentation", start);
            
            const invertedMask = maskImageHolder.threshold(0, 255, "Binary");
            if (growshrink > 0) {
                invertedMask.dilate(1, cv.Matrix.Ones(growshrink, growshrink));
            } else if (growshrink < 0) {
                invertedMask.erode(1, cv.Matrix.Ones(-growshrink, -growshrink));
            }
            if (blur > 0) {
                const adjusted = 2 * blur + 1;
                invertedMask.gaussianBlur([adjusted, adjusted]);
            }

            start = timeit("completed post processing", start);

            const resultBuffer = invertedMask.getData();
            invertedMask.release();
            await writePromise(socket, RESPONSE_HEADER);
            await writePromise(socket, getIntBuffer(resultBuffer.length));
            await writePromise(socket, resultBuffer);
            timeit(`Response time ${NUM_FRAMES}`, requestStart, true);
        }
        if (cvImageHolder !== null) {
            cvImageHolder.release();
        }
        if (maskImageHolder !== null) {
            maskImageHolder.release();
        }
    }

    async function main() {
        const nn = await bodyPix.load(CONFIG.modelOptions);
        const server = net.Server();
        let port = 3193; //getRandomPort();

        server.on('error', (e) => {
            if (e.code === 'EADDRINUSE') {
                setTimeout(() => {
                    server.close();
                    port = getRandomPort();
                    console.log(`Trying to listen on ${port}`)
                    server.listen(port, LOCALHOST);
                }, 100);
            }
        });
        server.on('listening', () => {
            const buf = new Buffer(4);
            buf.writeInt32LE(port);
            fs.writeFile(outputFile, buf, () => {});
            console.info(`Listening on ${LOCALHOST}:${port}`);
        });
        server.on('connection', (socket) => handleConnection(nn, socket));
        console.log(`Trying to listen on ${port}`);
        server.listen(port, LOCALHOST);
    }
    main();
})();
