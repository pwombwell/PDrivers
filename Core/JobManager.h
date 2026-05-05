#ifndef CORE_JOBMANAGER_H
#define CORE_JOBMANAGER_H

#include "Job.h"

#include "RLib/RLib.h"
#include "RLibX/Utils/DList.h"

class CoreWS;
class CoreJobWS;

class JobManager
{
public:
    JobManager(CoreWS& ws);

    CoreJobWS* current() const { return m_current; }
    CoreJobWS*& currentRef() { return m_current; }

    bool hasJobs() const { return !m_jobs.empty(); }

    // For iteration - maybe replace with a Visitor interface to emulate lambdas?
    // MyError forEachJobPreservingCurrent(JobVisitor& visitor);...
    CoreJobWS* firstJob() { return m_jobs.first(); }
    CoreJobWS* nextJob(CoreJobWS* job) { return m_jobs.next(job); }

    // Find a job by file handle.
    CoreJobWS* findJob(FileHandle handle); // `findjob`

    MyError selectJob(FileHandle file, const char* title, bool illustration,
                      FileHandle& previousJob);

    // Delete a job, informing pdriver. They can not object. `deletejob`
    MyError deleteJob(CoreJobWS* jobWS);

    MyError endJob(FileHandle handle);
    MyError abortJob(FileHandle handle);
    MyError reset();

private:
    CoreWS& m_ws;
    CoreJobWS* m_current; // `currentws`

    Utils::DList<CoreJobWS> m_jobs;
};

#endif
