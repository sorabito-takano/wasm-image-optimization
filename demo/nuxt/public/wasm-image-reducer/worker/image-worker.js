/// <reference lib="WebWorker" />
import { initWorker } from 'worker-lib';
// LibImage.js のインポート
// Note: この部分はビルド時に適切に解決される必要があります
let LibImage;
let libImageInstance = null;
/**
 * LibImage.js を動的にインポート
 */
async function initLibImage() {
    if (libImageInstance) {
        return;
    }
    try {
        // libImage.js を動的インポート
        const module = await import('../libImage/libImage.js');
        LibImage = module.default;
        // WASM バイナリの URL を設定
        const wasmUrl = new URL('../libImage/libImage.wasm', import.meta.url).href;
        // LibImage インスタンスを初期化
        libImageInstance = LibImage({
            locateFile: (path) => {
                if (path.endsWith('.wasm')) {
                    return wasmUrl;
                }
                return path;
            }
        });
    }
    catch (error) {
        console.error('Failed to initialize libImage:', error);
        throw error;
    }
}
/**
 * 画像変換処理を実行
 */
async function processImageInternal(image, format, quality, width, height) {
    if (!libImageInstance) {
        throw new Error('LibImage not initialized');
    }
    try {
        const libImage = await libImageInstance;
        const { optimize, releaseResult } = libImage;
        // libImage.optimize を呼び出し
        const result = optimize(image, width ?? 0, height ?? 0, quality, format);
        if (!result) {
            // 未対応形式の場合は入力をそのまま返す
            console.warn('Unsupported image format, returning original data');
            releaseResult();
            return {
                data: new Uint8Array(image),
                originalWidth: 0, // 実際の値は libImage から取得できない場合のデフォルト
                originalHeight: 0,
                width: width || 0,
                height: height || 0
            };
        }
        // 結果をコピーしてメモリを解放
        const resultData = {
            data: new Uint8Array(result.data),
            originalWidth: result.originalWidth,
            originalHeight: result.originalHeight,
            width: result.width,
            height: result.height
        };
        releaseResult();
        return resultData;
    }
    catch (error) {
        console.error('Image processing error:', error);
        // エラーの場合も処理されていることを示すため、元データを返すことを検討
        throw error;
    }
}
/**
 * Worker プロセス定義
 */
const WorkerProc = {
    /**
     * 画像処理を実行
     */
    async processImage(message) {
        const { type, payload } = message;
        try {
            if (type === 'reduce') {
                const { image, params } = payload;
                const { format, quality, width, height } = params;
                // LibImage が初期化されていない場合は初期化
                if (!libImageInstance) {
                    await initLibImage();
                }
                // 画像処理を実行
                const result = await processImageInternal(image, format, quality, width, height);
                // 結果を返す
                const response = {
                    success: true,
                    result
                };
                return response;
            }
            else {
                throw new Error(`Unknown message type: ${type}`);
            }
        }
        catch (error) {
            // エラーレスポンスを返す
            const response = {
                success: false,
                error: error instanceof Error ? error.message : String(error)
            };
            return response;
        }
    }
};
// worker-lib を使って Worker を初期化
initWorker(WorkerProc);
//# sourceMappingURL=image-worker.js.map