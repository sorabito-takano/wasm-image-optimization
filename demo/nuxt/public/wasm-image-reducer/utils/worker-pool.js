import { createWorker } from "worker-lib";
/**
 * グローバルワーカープールマネージャー
 */
class GlobalWorkerPoolManager {
    static instance = null;
    workerPool = null;
    workerCount = 8; // デフォルト値
    isInitialized = false;
    constructor() {
        // シングルトンパターン
    }
    /**
     * シングルトンインスタンスを取得
     */
    static getInstance() {
        if (!this.instance) {
            this.instance = new GlobalWorkerPoolManager();
        }
        return this.instance;
    }
    /**
     * ワーカー数を設定
     */
    async setWorkerCount(count) {
        if (count <= 0) {
            throw new Error("Worker count must be greater than 0");
        }
        if (!this.workerPool) {
            await this.initializePool();
        }
        this.workerPool?.setLimit(count);
    }
    /**
     * 現在のワーカー数を取得
     */
    getWorkerCount() {
        return this.workerCount;
    }
    /**
     * ワーカープールを初期化
     */
    async initializePool() {
        if (this.workerPool) {
            this.workerPool.close();
        }
        // Worker スクリプトのURLを生成
        const workerScript = new URL("./image-worker.js", import.meta.url).href;
        // ワーカープールを作成
        this.workerPool = createWorker(() => new Worker(workerScript, { type: "module" }), this.workerCount);
        // ワーカーの準備ができるまで待機
        await this.workerPool.waitReady();
        this.isInitialized = true;
    }
    /**
     * ワーカープールを取得（必要に応じて初期化）
     */
    async getPool() {
        if (!this.isInitialized || !this.workerPool) {
            await this.initializePool();
        }
        return this.workerPool;
    }
    /**
     * 画像処理タスクを実行
     */
    async processImage(image, params) {
        const pool = await this.getPool();
        const message = {
            type: "reduce",
            payload: {
                image,
                params,
            },
        };
        const response = await pool.execute("processImage", message);
        if (!response.success) {
            throw new Error(response.error || "Unknown worker error");
        }
        if (!response.result) {
            throw new Error("No result returned from worker");
        }
        return response.result;
    }
    /**
     * ワーカープールを破棄
     */
    async destroy() {
        if (this.workerPool) {
            await this.workerPool.waitAll();
            this.workerPool.close();
            this.workerPool = null;
            this.isInitialized = false;
        }
    }
}
// グローバルインスタンスをエクスポート
export const globalWorkerPool = GlobalWorkerPoolManager.getInstance();
// より直接的なAPIも提供
export const setWorkerCount = (count) => {
    globalWorkerPool.setWorkerCount(count);
};
export const getWorkerCount = () => {
    return globalWorkerPool.getWorkerCount();
};
export const processImage = async (image, params) => {
    return globalWorkerPool.processImage(image, params);
};
export const destroyWorkerPool = async () => {
    return globalWorkerPool.destroy();
};
//# sourceMappingURL=worker-pool.js.map