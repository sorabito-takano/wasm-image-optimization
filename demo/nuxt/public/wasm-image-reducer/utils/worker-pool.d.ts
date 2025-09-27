import type { ReduceParams, ReduceResult } from "../types/index.js";
/**
 * グローバルワーカープールマネージャー
 */
declare class GlobalWorkerPoolManager {
    private static instance;
    private workerPool;
    private workerCount;
    private isInitialized;
    private constructor();
    /**
     * シングルトンインスタンスを取得
     */
    static getInstance(): GlobalWorkerPoolManager;
    /**
     * ワーカー数を設定
     */
    setWorkerCount(count: number): Promise<void>;
    /**
     * 現在のワーカー数を取得
     */
    getWorkerCount(): number;
    /**
     * ワーカープールを初期化
     */
    private initializePool;
    /**
     * ワーカープールを取得（必要に応じて初期化）
     */
    private getPool;
    /**
     * 画像処理タスクを実行
     */
    processImage(image: ArrayBuffer, params: ReduceParams): Promise<ReduceResult>;
    /**
     * ワーカープールを破棄
     */
    destroy(): Promise<void>;
}
export declare const globalWorkerPool: GlobalWorkerPoolManager;
export declare const setWorkerCount: (count: number) => void;
export declare const getWorkerCount: () => number;
export declare const processImage: (image: ArrayBuffer, params: ReduceParams) => Promise<ReduceResult>;
export declare const destroyWorkerPool: () => Promise<void>;
export {};
//# sourceMappingURL=worker-pool.d.ts.map