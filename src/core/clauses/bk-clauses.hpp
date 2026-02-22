#include "beekeeper/clauses.hpp"

// command registry
extern std::unordered_map<std::string, clause> clauses_registry;

// Clause handler implementations
namespace beekeeper { namespace clauses {
command_streams
start(const clause_options& options, 
      const clause_subjects& subjects);

command_streams
stop(const clause_options& options, 
     const clause_subjects& subjects);

command_streams
restart(const clause_options& options, 
        const clause_subjects& subjects);

command_streams
status(const clause_options& options, 
       const clause_subjects& subjects);

command_streams
log(const clause_options& options, 
    const clause_subjects& subjects);

command_streams
clean(const clause_options& options, 
      const clause_subjects& subjects);

command_streams
help(const clause_options& options, 
     const clause_subjects& subjects);

command_streams
setup(const clause_options& options, 
      const clause_subjects& subjects);

command_streams
list (const clause_options& options,
      const clause_subjects& subjects);

command_streams
stat(const clause_options& options, 
     const clause_subjects& subjects);

command_streams
locate(const clause_options& options,
       const clause_subjects& subjects);

command_streams
autostartctl(const clause_options &options,
             const clause_subjects &subjects);

command_streams
compressctl(const clause_options &options,
            const clause_subjects &subjects);


} // namespace clauses
} // namespace beekeeper