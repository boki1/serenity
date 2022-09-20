/*
 * Copyright (c) 2021, the SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <LibCore/ArgsParser.h>
#include <LibCore/Stream.h>
#include <LibCore/System.h>
#include <LibCrypto/Checksum/Adler32.h>
#include <LibCrypto/Checksum/CRC32.h>
#include <LibMain/Main.h>
#include <string.h>

ErrorOr<int> serenity_main(Main::Arguments arguments)
{
    Vector<String> paths;
    char const* opt_algorithm = nullptr;

    Core::ArgsParser args_parser;
    args_parser.add_option(opt_algorithm, "Checksum algorithm (default 'crc32', use 'list' to list available algorithms)", "algorithm", '\0', nullptr);
    args_parser.add_positional_argument(paths, "File", "file", Core::ArgsParser::Required::No);
    args_parser.parse(arguments);

    auto algorithm = (opt_algorithm == nullptr) ? "crc32" : String(opt_algorithm).to_lowercase();

    auto available_algorithms = Vector<String> { "crc32", "adler32" };

    if (algorithm == "list") {
        outln("Available algorithms:");
        for (auto& available_algorithm : available_algorithms) {
            outln(available_algorithm);
        }
        exit(0);
    }

    if (!available_algorithms.contains_slow(algorithm)) {
        warnln("{}: Unknown checksum algorithm: {}", arguments.strings[0], algorithm);
        exit(1);
    }

    if (paths.is_empty())
        paths.append("-");

    bool fail = false;
    Array<u8, PAGE_SIZE> buffer;

    for (auto& path : paths) {
        auto file_or_error = Core::Stream::File::open_file_or_standard_stream(path, Core::Stream::OpenMode::Read);
        auto filepath = (path == "-") ? "/dev/stdin" : path;
        if (file_or_error.is_error()) {
            warnln("{}: {}: {}", arguments.strings[0], filepath, file_or_error.error());
            fail = true;
            continue;
        }
        auto file = file_or_error.release_value();

        auto stat_or_error = Core::System::stat(filepath);
        if (stat_or_error.is_error()) {
            warnln("{}: Failed to fstat {}: {}", arguments.strings[0], filepath, stat_or_error.error());
            fail = true;
            continue;
        }
        auto st = stat_or_error.release_value();

        if (algorithm == "crc32") {
            Crypto::Checksum::CRC32 crc32;
            while (!file->is_eof()) {
                auto data_or_error = file->read(buffer);
                if (data_or_error.is_error()) {
                    warnln("{}: Failed to read {}: {}", arguments.strings[0], filepath, data_or_error.error());
                    fail = true;
                    continue;
                }
                crc32.update(data_or_error.value());
            }
            outln("{:08x} {} {}", crc32.digest(), st.st_size, path);
        } else if (algorithm == "adler32") {
            Crypto::Checksum::Adler32 adler32;
            while (!file->is_eof()) {
                auto data_or_error = file->read(buffer);
                if (data_or_error.is_error()) {
                    warnln("{}: Failed to read {}: {}", arguments.strings[0], filepath, data_or_error.error());
                    fail = true;
                    continue;
                }
                adler32.update(data_or_error.value());
            }
            outln("{:08x} {} {}", adler32.digest(), st.st_size, path);
        } else {
            warnln("{}: Unknown checksum algorithm: {}", arguments.strings[0], algorithm);
            exit(1);
        }
    }

    return fail;
}
