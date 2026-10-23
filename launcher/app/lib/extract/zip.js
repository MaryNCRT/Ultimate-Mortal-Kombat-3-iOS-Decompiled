'use strict';

const fs = require('fs');
const path = require('path');
const zlib = require('zlib');

/**
 * Pure Node.js ZIP reader using standard 'fs' and 'zlib'.
 * Supports uncompressed (store = 0) and Deflate (method = 8).
 */
class ZipArchive {
    constructor(filePath) {
        this.filePath = filePath;
        this.fd = fs.openSync(filePath, 'r');
        this.entries = new Map();
        this._parse();
    }

    _parse() {
        const stat = fs.fstatSync(this.fd);
        const fileSize = stat.size;
        if (fileSize < 22) {
            throw new Error(`Invalid zip file: file size ${fileSize} too small`);
        }

        // Read up to 66KB from end of file to find End of Central Directory (EOCD)
        const readLen = Math.min(fileSize, 65536 + 22);
        const buf = Buffer.alloc(readLen);
        fs.readSync(this.fd, buf, 0, readLen, fileSize - readLen);

        let eocdOffset = -1;
        for (let i = readLen - 22; i >= 0; i--) {
            if (buf.readUInt32LE(i) === 0x06054b50) {
                eocdOffset = i;
                break;
            }
        }

        if (eocdOffset === -1) {
            throw new Error('Could not find End of Central Directory (EOCD) signature');
        }

        const totalEntries = buf.readUInt16LE(eocdOffset + 10);
        const cdSize = buf.readUInt32LE(eocdOffset + 12);
        const cdOffset = buf.readUInt32LE(eocdOffset + 16);

        // Read the Central Directory
        const cdBuf = Buffer.alloc(cdSize);
        fs.readSync(this.fd, cdBuf, 0, cdSize, cdOffset);

        let pos = 0;
        for (let i = 0; i < totalEntries && pos + 46 <= cdSize; i++) {
            const sig = cdBuf.readUInt32LE(pos);
            if (sig !== 0x02014b50) {
                break;
            }

            const compMethod = cdBuf.readUInt16LE(pos + 10);
            const crc = cdBuf.readUInt32LE(pos + 16);
            const compSize = cdBuf.readUInt32LE(pos + 20);
            const uncompSize = cdBuf.readUInt32LE(pos + 24);
            const nameLen = cdBuf.readUInt16LE(pos + 28);
            const extraLen = cdBuf.readUInt16LE(pos + 30);
            const commentLen = cdBuf.readUInt16LE(pos + 32);
            const localOffset = cdBuf.readUInt32LE(pos + 42);

            const filename = cdBuf.toString('utf8', pos + 46, pos + 46 + nameLen);
            this.entries.set(filename, {
                filename,
                compMethod,
                crc,
                compSize,
                uncompSize,
                localOffset,
            });

            pos += 46 + nameLen + extraLen + commentLen;
        }
    }

    namelist() {
        return Array.from(this.entries.keys());
    }

    has(filename) {
        return this.entries.has(filename);
    }

    get(filename) {
        return this.entries.get(filename);
    }

    read(filename) {
        const entry = this.entries.get(filename);
        if (!entry) {
            throw new Error(`File not found in archive: ${filename}`);
        }

        // Read local file header to find exact data start
        const headerBuf = Buffer.alloc(30);
        fs.readSync(this.fd, headerBuf, 0, 30, entry.localOffset);
        if (headerBuf.readUInt32LE(0) !== 0x04034b50) {
            throw new Error(`Corrupt local file header for ${filename}`);
        }

        const nameLen = headerBuf.readUInt16LE(26);
        const extraLen = headerBuf.readUInt16LE(28);
        const dataOffset = entry.localOffset + 30 + nameLen + extraLen;

        const rawData = Buffer.alloc(entry.compSize);
        if (entry.compSize > 0) {
            fs.readSync(this.fd, rawData, 0, entry.compSize, dataOffset);
        }

        if (entry.compMethod === 0) {
            // Stored
            return rawData;
        } else if (entry.compMethod === 8) {
            // Deflate
            return zlib.inflateRawSync(rawData);
        } else {
            throw new Error(`Unsupported compression method ${entry.compMethod} for ${filename}`);
        }
    }

    extractFile(filename, destPath) {
        const data = this.read(filename);
        fs.mkdirSync(path.dirname(destPath), { recursive: true });
        fs.writeFileSync(destPath, data);
    }

    close() {
        if (this.fd !== null) {
            fs.closeSync(this.fd);
            this.fd = null;
        }
    }
}

function openZip(filePath) {
    return new ZipArchive(filePath);
}

module.exports = {
    ZipArchive,
    openZip,
};
