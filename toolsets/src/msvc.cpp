#include "Component.h"
#include "Configuration.h"
#include "File.h"
#include "PendingCommand.h"
#include "Project.h"
#include "Toolset.h"
#include "dotted.h"

#include <algorithm>
#include <stack>

//https://blogs.msdn.microsoft.com/vcblog/2015/12/03/c-modules-in-vs-2015-update-1/
// Enable modules support for MSVC
//"/experimental:module /module:stdIfcDir \"$(VC_IFCPath)\" /module:search obj/modules/"

std::string MsvcToolset::getLibNameFor(Component &component)
{
    return "lib" + getNameFor(component) + ".lib";
}

std::string MsvcToolset::getExeNameFor(Component &component)
{
    return getNameFor(component) + ".exe";
}

MsvcToolset::MsvcToolset() 
: compiler("cl.exe")
, linker("link.exe")
, archiver("lib.exe")
{

}

void MsvcToolset::SetParameter(const std::string& key, const std::string& value) {
  if (key == "compiler") compiler = value;
  else if (key == "linker") linker = value;
  else if (key == "archiver") archiver = value;
  else throw std::runtime_error("Invalid parameter for MSVC toolchain: " + key);
}

void MsvcToolset::CreateCommandsForUnity(Project &project) {}

void MsvcToolset::CreateCommandsFor(Project &project)
{
    for(auto &p : project.components)
    {
        auto &component = p.second;
        std::string includes;
        for(auto &d : getIncludePathsFor(component))
        {
            includes += " /I" + filesystem::relative(d).string();
        }

        // TODO: modules: -fmodules-ts --precompile  -fmodules-cache-path=<directory>-fprebuilt-module-path=<directory>
        filesystem::path outputFolder = component.root;
        std::vector<File *> objects;
        for(auto &f : component.files)
        {
            if(!File::isTranslationUnit(f->path))
                continue;
            filesystem::path temp = (f->path.string().substr(component.root.string().size()) + ".obj");
            filesystem::path outputFile = std::string("obj") / outputFolder / temp;
            File *of = project.CreateFile(component, outputFile);
            std::shared_ptr<PendingCommand> pc = std::make_shared<PendingCommand>(compiler + " /c /EHsc " + Configuration::Get().compileFlags + includes + " /Fo" + filesystem::weakly_canonical(outputFile).string() + " " + filesystem::weakly_canonical(f->path).string());
            objects.push_back(of);
            pc->AddOutput(of);
            std::unordered_set<File *> d;
            std::stack<File *> deps;
            deps.push(f);
            size_t index = 0;
            while(!deps.empty())
            {
                File *dep = deps.top();
                deps.pop();
                pc->AddInput(dep);
                for(File *input : dep->dependencies)
                    if(d.insert(input).second)
                        deps.push(input);
                index++;
            }
            pc->Check();
            component.commands.push_back(pc);
        }
        if(!objects.empty())
        {
            std::string command;
            filesystem::path outputFile;
            std::shared_ptr<PendingCommand> pc;
            if(component.type == "library")
            {
                outputFile = "lib\\" + getLibNameFor(component);
                command = archiver + " " + filesystem::weakly_canonical(outputFile).string();
                for(auto &file : objects)
                {
                    command += " " + file->path.string();
                }
                pc = std::make_shared<PendingCommand>(command);
            }
            else
            {
                outputFile = "bin\\" + getExeNameFor(component);
                command = linker + " /OUT:" + outputFile.string();

                for(auto &file : objects)
                {
                    command += " " + file->path.string();
                }
                command += " /LIBPATH:lib";
                std::vector<std::vector<Component *>> linkDeps = GetTransitiveAllDeps(component);
                std::reverse(linkDeps.begin(), linkDeps.end());
                for(auto d : linkDeps)
                {
                    for(auto &c : d)
                    {
                        if(c != &component && !c->isHeaderOnly())
                        {
                            command += " " + c->root.string();
                        }
                    }
                }
                pc = std::make_shared<PendingCommand>(command);
                for(auto &d : linkDeps)
                {
                    for(auto &c : d)
                    {
                        if(c != &component && !c->isHeaderOnly())
                        {
                            pc->AddInput(project.CreateFile(*c, "lib\\" + getLibNameFor(*c)));
                        }
                    }
                }
            }
            File *libraryFile = project.CreateFile(component, outputFile);
            pc->AddOutput(libraryFile);
            for(auto &file : objects)
            {
                pc->AddInput(file);
            }
            pc->Check();
            component.commands.push_back(pc);
            if(component.type == "unittest")
            {
                command = outputFile.string();
                pc = std::make_shared<PendingCommand>(command);
                outputFile += ".log";
                pc->AddInput(libraryFile);
                pc->AddOutput(project.CreateFile(component, outputFile.string()));
                pc->Check();
                component.commands.push_back(pc);
            }
        }
    }
}
