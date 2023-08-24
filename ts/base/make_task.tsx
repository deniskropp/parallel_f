import { Task } from './task'

export function make_task(f: Function, args: any[]) {
    return new Task(f, args)
}
