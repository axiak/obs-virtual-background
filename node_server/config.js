module.exports = {
    segmentationOptions: {
        flipHorizontal: false,
        internalResolution: 'high',
        segmentationThreshold: 0.7,
    },
    modelOptions: {
        architecture: 'MobileNetV1',
        //architecture: 'ResNet50',
        outputStride: 16,    
        multiplier: 0.75,
        quantBytes: 2,
    },
    debugTimings: false
};
