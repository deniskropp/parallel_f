import { Task } from './task'

export function make_task(f: Function, args: any[]) {
    console.log('make_task', f, args)

    return new Task(f, args)
}
