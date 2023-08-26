import { Task } from './task'

export class TaskList {
    private all: Set<Task>

    constructor() {
        this.all = new Set
    }

    append(task: Task, deps?: Task[]) {
        if (deps === undefined)
            deps = []

        console.log(`TaskList::append(${task.fname()}) <= ${deps.map(t => t.fname())})`)

        this.all.add(task)
    }

    async flush() {
        console.log('TaskList::flush()')

        throw new Error('unimplemented')
    }

    async finish() {
        console.log('TaskList::finish()')

        throw new Error('unimplemented')
    }
}
