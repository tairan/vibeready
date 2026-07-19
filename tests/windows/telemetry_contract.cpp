#include "telemetry.h"

#include <windows.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {

int Fail(const std::string& message) {
    std::cerr << message << "\n";
    return 1;
}

std::vector<std::string> ReadLines(const std::filesystem::path& path) {
    std::vector<std::string> lines;
    std::ifstream file(path, std::ios::binary);
    std::string line;
    while (std::getline(file, line)) {
        if (!line.empty()) {
            lines.push_back(line);
        }
    }
    return lines;
}

vibeready::telemetry::Context TestContext(const std::filesystem::path& root, bool consentEnabled) {
    vibeready::telemetry::Context context;
    context.appDataDir = root.wstring();
    context.appVersion = L"0.1.0-test";
    context.osMajorVersion = L"11";
    context.architecture = L"x64";
    context.language = L"en";
    context.consentEnabled = consentEnabled;
    return context;
}

int RunTransportContract(const std::wstring& endpoint) {
    std::filesystem::path root =
        std::filesystem::temp_directory_path() / (L"VibeReadyTelemetryTransport-" + vibeready::telemetry::NewId());
    std::filesystem::path queuePath = root / L"telemetry-queue.jsonl";
    std::error_code ignored;

    try {
        std::filesystem::create_directories(root);
        vibeready::telemetry::Context context = TestContext(root, true);
        context.endpoint = endpoint;
        vibeready::telemetry::Initialize(context);

        for (int index = 0; index < 45; ++index) {
            vibeready::telemetry::Track(L"app_opened", {
                vibeready::telemetry::Text(L"launch_result", L"transport-test"),
            });
        }

        for (int attempt = 0; attempt < 200; ++attempt) {
            if (attempt % 10 == 0) {
                vibeready::telemetry::FlushAsync();
            }
            if (std::filesystem::exists(queuePath) && std::filesystem::file_size(queuePath) == 0) {
                vibeready::telemetry::SetConsent(false);
                std::filesystem::remove_all(root, ignored);
                std::cout << "VibeReady telemetry transport contract passed.\n";
                return 0;
            }
            Sleep(100);
        }
    } catch (const std::exception& error) {
        std::filesystem::remove_all(root, ignored);
        return Fail(std::string("Telemetry transport test failed: ") + error.what());
    } catch (...) {
        std::filesystem::remove_all(root, ignored);
        return Fail("Telemetry transport test failed with a non-standard exception.");
    }

    vibeready::telemetry::SetConsent(false);
    std::filesystem::remove_all(root, ignored);
    return Fail("Telemetry queue did not drain through the local batch endpoint.");
}

}  // namespace

int wmain(int argc, wchar_t* argv[]) {
    if (argc == 3 && std::wstring(argv[1]) == L"--transport") {
        return RunTransportContract(argv[2]);
    }

    std::filesystem::path root =
        std::filesystem::temp_directory_path() / (L"VibeReadyTelemetryTest-" + vibeready::telemetry::NewId());
    std::filesystem::path queuePath = root / L"telemetry-queue.jsonl";
    std::error_code ignored;

    try {
        std::filesystem::create_directories(root);

        vibeready::telemetry::Initialize(TestContext(root, false));
        vibeready::telemetry::Track(L"app_opened", {
            vibeready::telemetry::Text(L"launch_result", L"test"),
        });
        if (std::filesystem::exists(queuePath)) {
            return Fail("Consent-off telemetry created a queue file.");
        }

        vibeready::telemetry::SetConsent(true);
        vibeready::telemetry::Track(L"app_opened", {
            vibeready::telemetry::Text(L"launch_result", L"test"),
        });
        std::vector<std::string> lines = ReadLines(queuePath);
        if (lines.size() != 1 || lines.front().find("\"consent_state\":\"enabled\"") == std::string::npos) {
            return Fail("Consent-on telemetry did not persist the expected anonymous event.");
        }

        vibeready::telemetry::Track(L"unknown_event");
        vibeready::telemetry::Track(L"app_opened", {
            vibeready::telemetry::Text(L"full_path", L"C:\\Users\\example\\secret.txt"),
        });
        if (ReadLines(queuePath).size() != 1) {
            return Fail("Unknown or sensitive telemetry was added to the queue.");
        }
        vibeready::telemetry::Track(L"app_opened", {
            vibeready::telemetry::Text(L"launch_result", std::wstring(9 * 1024, L'x')),
        });
        if (ReadLines(queuePath).size() != 1) {
            return Fail("Oversized telemetry was added to the queue.");
        }

        for (int index = 0; index < 220; ++index) {
            vibeready::telemetry::Track(L"app_opened", {
                vibeready::telemetry::Text(L"launch_result", L"queue-cap-test"),
            });
        }
        lines = ReadLines(queuePath);
        if (lines.size() > 200) {
            return Fail("Telemetry queue exceeded the 200-event limit.");
        }
        if (std::filesystem::file_size(queuePath) > 256 * 1024) {
            return Fail("Telemetry queue exceeded the 256 KiB limit.");
        }

        vibeready::telemetry::SetConsent(false);
        if (std::filesystem::exists(queuePath)) {
            return Fail("Disabling consent did not clear the telemetry queue.");
        }

        std::filesystem::path invalidRoot = root / L"not-a-directory";
        {
            std::ofstream file(invalidRoot, std::ios::binary);
            file << "file blocks directory creation";
        }
        vibeready::telemetry::Initialize(TestContext(invalidRoot, true));
        vibeready::telemetry::Track(L"app_opened", {
            vibeready::telemetry::Text(L"launch_result", L"filesystem-failure"),
        });
    } catch (const std::exception& error) {
        std::filesystem::remove_all(root, ignored);
        return Fail(std::string("Telemetry operation escaped into the application flow: ") + error.what());
    } catch (...) {
        std::filesystem::remove_all(root, ignored);
        return Fail("Telemetry operation escaped a non-standard exception into the application flow.");
    }

    std::filesystem::remove_all(root, ignored);
    std::cout << "VibeReady telemetry contract passed.\n";
    return 0;
}
