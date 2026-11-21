#define CATCH_CONFIG_RUNNER

#include <clocale>

#include <QCoreApplication>

#include <catch2/catch_all.hpp>

/*
 * Main entry point of the executable.
 */
int main(int argc, char *argv[]);

int main(int argc, char *argv[])
{
    // initialize the Qt application of the host tests
    const QCoreApplication application(argc, argv);

    // format the numbers of the test reports in the C locale
    [[maybe_unused]] const char *numericLocale = std::setlocale(LC_NUMERIC, "C");

    // initialize new session
    Catch::Session session;

    // build a new configuration with high verbosity
    session.configData().showSuccessfulTests = true;
    session.configData().verbosity = Catch::Verbosity::High;
    session.configData().runOrder = Catch::TestRunOrder::Declared;

    // apply user configurations
    const int returnCode = session.applyCommandLine(argc, argv);
    if (returnCode != 0) {
        return returnCode;
    }

    // run the session
    return session.run();
}
