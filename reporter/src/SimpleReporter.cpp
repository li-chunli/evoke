#include "PendingCommand.h"
#include "Reporter.h"

#include <iostream>

void SimpleReporter::SetConcurrencyCount(size_t)
{
}

void SimpleReporter::SetRunningCommand(size_t, std::shared_ptr<PendingCommand> command)
{
    if(command)
        std::cout << command->commandToRun << "\n"
                  << std::flush;
}

void SimpleReporter::ReportFailure(std::shared_ptr<PendingCommand> command, int error, const std::string &errors)
{
    if(error)
    {
        std::cout << "Error while running " << command->commandToRun << "\n";
    }
    std::cout << errors << "\n"
              << std::flush;
}
