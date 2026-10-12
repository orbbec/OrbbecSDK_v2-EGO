/**
 * @file FileIo.h
 * @brief Minimal text file I/O helpers for the host-side parameter tooling.
 *
 * File access is isolated in this small module (rather than inside the parsing /
 * serialization code) so a firmware build without a filesystem can simply drop
 * FileIo.cpp. The parsing and generation layers stay pure string-to-string.
 */

#pragma once

#include <string>

namespace camadapt {

/**
 * @brief Read an entire text file into a string.
 *
 * @param path  File path.
 * @return File contents (bytes as-is, typically UTF-8).
 * @throws std::runtime_error if the file cannot be opened or read.
 */
std::string readTextFile(const std::string &path);

/// Storage convention: a file this library persists must have an even byte count
/// (the device protocol's length field counts half-words). A single trailing '\n'
/// is used as the pad byte: it is whitespace for both the JSONC reader and the YAML
/// reader, it only ever changes the file tail, and it is idempotent.
constexpr char kStoragePadByte = '\n';

/**
 * @brief Append @ref kStoragePadByte to @p content iff its size() is odd.
 *
 * Pure string helper for the storage convention above; the input is returned
 * unchanged when it already has an even byte count (which makes the operation
 * idempotent: `padToEvenByteSize(padToEvenByteSize(s)) == padToEvenByteSize(s)`).
 *
 * @param content  Serialized file content.
 * @return @p content, plus one trailing '\n' when the byte count was odd.
 */
std::string padToEvenByteSize(std::string content);

/**
 * @brief Check the storage convention on a file already on disk.
 *
 * Self-check for tooling and tests: reads the file and reports whether its size
 * in bytes is even. Deliberately does NOT throw for a missing/unreadable file
 * (the write-path helper @ref writeTextFile reports failure through its return
 * value instead), it simply returns false.
 *
 * @param path  File path.
 * @return true when the file exists and its byte count is even; false otherwise.
 */
bool hasEvenByteSize(const std::string &path);

/**
 * @brief Write a string to a text file (truncates existing content).
 *
 * The content actually written is `padToEvenByteSize(content)`, and the file is
 * opened in **binary** mode ("wb") so that no EOL translation can happen on
 * Windows (the library's shipped outputs are pure LF today).
 *
 * The storage convention: a file this library persists must have an even byte
 * count, because the device-side SDK protocol carries file contents as
 * `header + body` with `len` counting **half-words** of the body
 * (`body_bytes = 2 * len`). An odd byte count is padded with one trailing '\n'
 * (only this library's own output is guaranteed even; files written by other
 * teams are out of scope by decision).
 *
 * Padding is harmless to every reader: a trailing '\n' is whitespace for both the
 * JSONC reader and the YAML reader, it only ever changes the file tail, and it is
 * idempotent. Note also that @ref computeFingerprint and the params
 * `body_fingerprint` are computed from **parsed semantic fields**, never from the
 * raw file bytes, so a padded file validates and hashes exactly as before.
 *
 * @param path     File path.
 * @param content  Content to write.
 * @return true on success, false on failure.
 */
bool writeTextFile(const std::string &path, const std::string &content);

}  // namespace camadapt
