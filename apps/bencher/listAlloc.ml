type 'a tree = Empty | Node of 'a tree

let rec make d =
  if d = 0 then Node(Empty)
  else let d = d - 1 in Node(make d)

let rec check = function Empty -> 0 | Node(l) -> 1 + check l

let rec loop_depths size =
  let c = ref 0 in
  for i = 0 to size do
    c := !c + check (make (size-i))
  done;
  Printf.printf "%i\n" !c


let () =
  flush stdout;
  loop_depths 20000
