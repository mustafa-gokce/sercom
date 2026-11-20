#define CATCH_CONFIG_RUNNER

#include <catch2/catch_all.hpp>

/*
 * Main entry point of the executable.
 */
int main(int argc, char *argv[]);

int main(int argc, char *argv[])
{
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
