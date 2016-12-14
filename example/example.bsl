#!/usr/bin/env bsl

data Int where {}

data Unit where {
  Unit:Unit
}

data Bool where {
  False:Bool;
  True:Bool
}

data Maybe a where {
  Just:forall a.a->Maybe a;
  Nothing:forall a.Maybe a
}

data List a where {
  Nil:forall a.List a;
  Cons:forall a.a->List a->List a
}

data IOImpl a where {
  Read:forall a.(Maybe Int->a)->IOImpl a;
  Write:forall a.Int->a->IOImpl a
}

data IO a where {
  Pure:forall a.a->IO a;
  Free:forall a.IOImpl (IO a)->IO a
}

let fmap = \f -> \x -> case x of {
  Write s k -> Write s (f k);
  Read k -> Read (\s -> f (k s))
} in

let return = Pure in
rec bind = \x -> \f -> case x of {
  Pure x -> f x;
  Free x -> Free (fmap (\y -> bind y f) x)
} in

let getInt = Free (Read (\x -> return x)) in
let putInt = \x -> Free (Write x (return Unit)) in

rec runIO = \x -> case x of {
  Pure x -> x;
  Free x -> case x of {
    Write c x -> let _ = ffi ` (printf("%d\n", (int) $c), NULL) ` in (runIO x);
    Read g -> let x:Unit->Maybe Int = \x -> ffi ` (scanf("%d",&$x) == 1 ? BSL_RT_CALL($Just, $x) : $Nothing) ` in runIO (g (x Unit))
  }
} in

let not = \x -> case x of {
  True -> False;
  False -> True
} in

let less:Int->Int->Bool = \a -> \b -> ffi ` ((((int) $a) < ((int) $b)) ? $True : $False) ` in

rec concat = \a -> \b -> case a of {
  Nil -> b;
  Cons x xs -> Cons x (concat xs b)
} in
rec filter = \list -> \f -> case list of {
  Nil -> Nil;
  Cons x xs -> case f x of {
    True -> Cons x (filter xs f);
    False -> filter xs f
  }
} in
let sort = \less ->
  rec sortLess = \list -> case list of {
    Nil -> Nil;
    Cons x xs -> concat (sortLess (filter xs (\y -> not (less x y))))
                 (Cons x (sortLess (filter xs (less x) )))
  } in sortLess
in

let reverse = \l ->
  rec reverse' = \xs -> \l -> case l of {
    Cons x t -> reverse' (Cons x xs) t;
    Nil -> xs
  } in reverse' Nil l
in

let getList =
  rec getList' = \xs -> bind getInt \x -> case x of {
    Just x -> getList' (Cons x xs);
    Nothing -> return (reverse xs)
  } in getList' Nil
in
rec putList = \list -> case list of {
  Nil -> return Unit;
  Cons x xs -> bind (putInt x) \_ ->
               putList xs
} in

let main = bind getList \list ->
                putList (sort less list)
in runIO main

