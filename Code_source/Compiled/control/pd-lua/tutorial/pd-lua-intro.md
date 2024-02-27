
# A Quick Introduction to Pd-Lua

Albert Gräf \<<aggraef@gmail.com>\>  
Computer Music Dept., Institute of Art History and Musicology  
Johannes Gutenberg University (JGU) Mainz, Germany  
February 2023

This document is licensed under [CC BY-SA 4.0](https://creativecommons.org/licenses/by-sa/4.0/). Other formats: [Markdown](https://github.com/agraef/pd-lua/blob/master/tutorial/pd-lua-intro.md) source, [PDF](https://github.com/agraef/pd-lua/blob/master/pdlua/tutorial/pd-lua-intro.pdf)  
Permanent link: <https://agraef.github.io/pd-lua/tutorial/pd-lua-intro.html>

## Why Pd-Lua?

Pd's facilities for data structures, iteration, and recursion are somewhat limited, thus sooner or later you'll probably run into a problem that can't be easily solved by a Pd abstraction any more. At this point you'll have to consider writing an external object (or just *external*, for short) in a "real" programming language instead. Pd externals are usually programmed using C, the same programming language that Pd itself is written in. But novices may find C difficult to learn, and the arcana of Pd's C interface may also be hard to master.

Enter Pd-Lua, the Pd programmer's secret weapon, which lets you develop your externals in the [Lua](https://www.lua.org/) scripting language. Pd-Lua was originally written by Claude Heiland-Allen and has since been maintained by a number of other people in the Pd community. Lua, from [PUC Rio](http://www.puc-rio.br/), is open-source (under the MIT license), mature, very popular, and supported by a large developer community. It is a small programming language, but very capable, and is generally considered to be relatively easy to learn. For programming Pd externals, you'll also need to learn a few bits and pieces which let you interface your Lua functions to Pd, as explained in this tutorial, but programming externals in Lua is still quite easy and a lot of fun.

Using Pd-Lua, you can program your own externals ranging from little helper objects to full-blown sequencers and algorithmic composition tools. Pd-Lua only allows you to program control objects at this time (for doing dsp, you might consider using [Faust](https://faust.grame.fr/) instead), but it gives you access to Pd arrays and tables, as well as a number of other useful facilities such as clocks and receivers, which we'll explain in some detail. Pd-Lua also ships with a large collection of instructive examples which you'll find helpful when exploring its possibilities.

Note that we can't possibly cover Pd or the Lua language themselves here, so you'll have to refer to other online resources to learn about those. In particular, check out the Lua website, which has extensive [documentation](https://www.lua.org/docs.html) available, and maybe have a look at Derek Banas' [video tutorial](https://www.youtube.com/watch?v=iMacxZQMPXs) for a quick overview of Lua. For Pd, we recommend the Pd FLOSS Manual at https://flossmanuals.net/ to get started.

## Installation

Pd-Lua works inside any reasonably modern Pd flavor. This encompasses vanilla [Pd](http://msp.ucsd.edu/software.html), of course, but also [Purr Data](https://agraef.github.io/purr-data/) which includes an up-to-date version of Pd-Lua for Lua 5.4 and has it enabled by default, so you should be ready to go immediately; no need to install anything else. The same is true for [plugdata](https://plugdata.org/) (version 0.6.3 or later), a Pd flavor which can also run as a plug-in inside a DAW.

With vanilla Pd, you can install the pdlua package from Deken (not recommended, because at the time of this writing that's a really old version based on Lua 5.1). The official [Debian](https://salsa.debian.org/multimedia-team/pd/pd-lua) package, maintained by IOhannes Zmölnig, is based on Lua 5.2. If you want to use a reasonably up-to-date Lua version, your best bet is to get Pd-Lua from the author's [Github repository](https://github.com/agraef/pd-lua), which has been updated to work with Lua 5.3 and later. Compilation instructions are in the README, and you'll also find some Mac and Windows binaries there. In either case, after installing Pd-Lua you also have to add `pdlua` to Pd's startup libraries.

If all is well, you should see a message like the following in the Pd console (note that for vanilla Pd you'll have to switch the log level to 2 or more to see that message):

~~~
pdlua 0.11.3 (GPL) 2008 Claude Heiland-Allen, 2014 Martin Peach et al.
pdlua: compiled for pd-0.53 on Jan 16 2023 23:36:09
Using lua version 5.4
~~~

This will also tell you the Lua version that Pd-Lua is using, so that you can install a matching version of the stand-alone Lua interpreter if needed. Lua should be readily available from your package repositories on Linux, and for Mac and Windows you can find binaries on the Lua website. In the following we generally assume that you're using Lua 5.3 or later.

If all is not well and you do *not* see that message, then most likely Pd-Lua refused to load because the Lua library is missing. This shouldn't happen if you installed Pd-Lua from a binary package, but if it does then you'll have to manually install the right version of the Lua library to make Pd-Lua work (5.1 for the Deken package, 5.2 for the Debian package, and usually 5.4 otherwise). Make sure that you install the package with the Lua *library* in it; on Debian, Ubuntu and their derivatives this will be something like liblua5.4-0.

## A basic example

With that out of the way, let's have a look at the most essential parts of a Lua external. To make an external, say `foo`, loadable by Pd-Lua, you need to put it into a Lua script, which is simply a text file with the right name (which must be the same as the object name, `foo` in this case) and extension (which needs to be `.pd_lua`), so the file name will be `foo.pd_lua` in this example.

Any implementation of an object must always include:

- a call to the `pd.Class:new():register` method which registers the object class with Pd (this should always be the first line of the script, other than comments)

- a definition of the `initialize` method for your object class

Here is a prototypical example (this is the contents of the foo.pd_lua file):

~~~lua
local foo = pd.Class:new():register("foo")

function foo:initialize(sel, atoms)
   return true
end
~~~

Note that in the first line we called `pd.Class:new():register` with the name of the object class as a string, which *must* be the same as the basename of the script, otherwise Pd's loader will get very confused, create the wrong object class, print a (rather cryptic) error message, and won't be able to create the object.

We also assigned the created class (which is represented as a Lua table) to a variable `foo` (which we made local to the script file here, as explained below). We need that variable as a qualifier for the methods of the object class, including `initialize`. You can actually name that variable whatever you want, as long as you use that name consistently throughout the script. This can be useful at times, if the actual class name you chose, as it is known to Pd and set with `pd.Class:new():register` (as well as being the basename of your .pd_lua script), is a jumble of special characters such as `fo:o#?!`, which isn't a valid Lua identifier.

Next comes the `initialize` method, which is implemented as a Lua function, prefixing the method name with the name of the class variable we created above and a colon, i.e., `foo:initialize`. (This *colon syntax* is used for all functions that represent *methods*, which receive the called object as an implicit `self` parameter; please check the section on function definitions in the Lua manual for details.) As a bare minimum, as is shown here, this method *must* return `true`, otherwise the loader will assume that the object creation has failed, and will complain that it couldn't create the object with an error message.

We mention in passing here that Pd-Lua also provides a parameter-less `postinitialize` method which can be used to execute code after the object has been created, but before the object starts processing messages. We'll see an example of this method later.

---

**NOTE:** Pd-Lua runs *all* Lua objects in the same instance of the Lua interpreter. Therefore, as a general guideline, we want to keep the global name space tidy and clean. That's why we made `foo` a local variable, which means that its scope is confined to this single script. Note that this isn't needed for the member variables and methods, as these are securely stowed away inside the object and not accessible from the outside anyway, if the class variable is `local`. But the same caveat applies to all variables and functions in the script file that might be needed to implement the object, so normally you want to mark these as `local`, too (or turn them into member variables and methods, if that seems more appropriate).

We mention in passing that global variables and functions may also have their uses if you need to share a certain amount of global state between different Lua objects. But even then it's usually safer to have the objects communicate with each other behind the scenes using receivers, which we'll explain later.

---

To actually use the object class we just created, Pd needs be able to find our foo.pd_lua file. We'll discuss different approaches in the following section, but the easiest way to achieve this is to just drop foo.pd_lua into the directory that your patch is in (say, `pd-lua` in your home directory). Now we can just create our first `foo` object (hit Ctrl+1, then type the object name `foo`), and we should see something like this:

![First light](01-first-light.png)

Hooray, it works! :)) Well, this object doesn't do anything right now, so let's equip it with a single inlet/outlet pair. This is what the `initialize` method is for, so we have to edit that method accordingly.

NB: If you closed the editor already and don't remember where the file is, you can just right-click the object and choose `Open`, which will open the .pd_lua file in your favorite text editor, as configured in your desktop and/or shell environment.

~~~lua
local foo = pd.Class:new():register("foo")

function foo:initialize(sel, atoms)
   self.inlets = 1
   self.outlets = 1
   return true
end
~~~

Note that, as we already mentioned, the `self` variable here is an implicit parameter of any Lua method, which refers to the object itself. Every Pd-Lua object has two member variables `inlets` and `outlets` which let us specify the number of inlets and outlets our object should have. This needs to be done when the object is initialized; afterwards, the number of inlets and outlets is set in stone and can't be changed any more.

Next, we have to make sure that Pd picks up our edited class definition. Since the Pd-Lua loader will never reload the .pd_lua file for any given object class during a Pd session, we will have to save the patch, quit Pd, relaunch it and reopen the patch:

![Inlet and outlet](02-inoutlets.png)

So there, we got our single inlet/outlet pair now. To do anything with these, we finally have to add some *message handlers* to our object. Say, for instance, we want to handle a bang message by incrementing a counter and outputting its current value to the outlet. We first have to initialize the counter value in the `initialize` method. As we want each `foo` object to have its own local counter value, we create the counter as a member variable:

~~~lua
function foo:initialize(sel, atoms)
   self.inlets = 1
   self.outlets = 1
   self.counter = 0
   return true
end
~~~

It's not necessary to declare the `self.counter` variable in any way, just give it an initial value and be done with it. Finally, we have to add a method for the bang message, which looks as follows:

~~~lua
function foo:in_1_bang()
   self.counter = self.counter + 1
   self:outlet(1, "float", {self.counter})
end
~~~

We'll dive into the naming conventions for message handlers later, but note that `in_1` specifies the first (and only) inlet and `bang` the kind of message we expect. In the body of the method we increment the `self.counter` value and output its new value on the first (and only) outlet. This is done by the predefined `self:outlet` method which takes three arguments: the outlet number, the (Pd) data type to output, and the output value itself. (In general, it's possible to have multiple values there, e.g., when outputting a list value. Therefore the output value is always specified as a Lua table, hence the curly braces around the float output value.)

Throwing everything together, our Lua external now looks as follows:

~~~lua
local foo = pd.Class:new():register("foo")

function foo:initialize(sel, atoms)
   self.inlets = 1
   self.outlets = 1
   self.counter = 0
   return true
end

function foo:in_1_bang()
   self.counter = self.counter + 1
   self:outlet(1, "float", {self.counter})
end
~~~

So let's relaunch Pd, reload the patch again, and add some GUI elements to test it out:

![Basic example](03-basic-example.png)

Note that this is still a very basic example. While the example is complete and fully functional, we have barely scratched the surface here. Pd-Lua also allows you to process an object's creation arguments (employing the `atoms` parameter of the `initialize` method, which we didn't use above), log messages and errors in the Pd console, create handlers for different types of input messages, output data to different outlets, work with Pd arrays, clocks, and receivers, and even do some live coding. We will dive into each of these topics in the following sections.

## Where your Lua files go

As already mentioned, the externals (.pd_lua files) themselves can go either into the directory of the patch using the external, or into any other directory on Pd's search path (on Linux, this generally includes, ~/.pd-externals, or  ~/.pd-l2ork-externals when running pd-l2ork or purr-data).

The Lua loader temporarily sets up Lua's `package.path` so that it includes the directory with the external, so you can put any Lua modules (.lua files) required by the external into that directory.

If you need/want to use Lua libraries from other locations (which aren't on the standard Lua `package.path`), then you'll have to set up the `LUA_PATH` environment variable accordingly. When using [LuaRocks](https://luarocks.org/), Lua's most popular package manager, it usually takes care of this for you when set up properly. Otherwise you can set `LUA_PATH` manually in your system startup files, such as ~/.bashrc or ~/.xprofile on Linux. E.g.:

~~~shell
export LUA_PATH=~/lua/'?.lua;;'
~~~

Note that `?` is a placeholder for the module name, the semicolon `;` can be used to separate different locations, and a double semicolon `;;` adds Lua's standard search path (make sure that you quote those special characters so that the shell doesn't try to interpret them). You should *always* include the double semicolon somewhere, otherwise the Lua interpreter won't be able to find its standard library modules any more. Also note that you may want to place the `;;` in front of the path instead, if the standard locations are to be searched before your custom ones.

## Creation arguments

Besides the implicit `self` argument, the `initialize` method has two additional parameters:

- `sel`, the selector argument, is a string which contains the Pd name of the object class. You probably won't need this, unless you want to use it for error reporting, or if you have a generic setup function for several related object classes. We won't go into this here.

- `atoms` is a Lua table which contains all the arguments (Pd "atoms", i.e., numbers or strings) an object was created with. `#atoms` gives you the number of creation arguments (which may be zero if none were specified), `atoms[1]` is the first argument, `atoms[2]` the second, and so on. As usual in Lua, if the index `i` runs past the last argument, `atoms[i]` returns `nil`.

For instance, let's say that we want to equip our `foo` object with an optional creation argument, a number, to set the initial counter value. This can be done as follows:

~~~lua
function foo:initialize(sel, atoms)
   self.inlets = 1
   self.outlets = 1
   if type(atoms[1]) == "number" then
      self.counter = atoms[1]
   else
      self.counter = 0
   end
   return true
end
~~~

Here we check that the first creation argument is a number. In that case we use it to initialize the `counter` member variable, otherwise a default value of 0 is set. Note that if there is no creation argument, `atoms[1]` will be `nil` which is of type `"nil"`, in which case the zero default value will be used.

Note that currently our `bang` handler outputs the value *after* incrementing it, which seems a bit awkward now that we can actually specify the counter's start value. Let's rework that method so that it spits out the *current* value before incrementing it:

~~~lua
function foo:in_1_bang()
   self:outlet(1, "float", {self.counter})
   self.counter = self.counter + 1
end
~~~

Note that it's perfectly fine to invoke `self:outlet` at any point in the method.

While we're at it, we might as well add an optional second creation argument to specify the step value of the counter. Try doing that on your own, before peeking at the solution below!

Got it? Good. Here is our final script:

~~~lua
local foo = pd.Class:new():register("foo")

function foo:initialize(sel, atoms)
   self.inlets = 1
   self.outlets = 1
   if type(atoms[1]) == "number" then
      self.counter = atoms[1]
   else
      self.counter = 0
   end
   if type(atoms[2]) == "number" then
      self.step = atoms[2]
   else
      self.step = 1
   end
   return true
end

function foo:in_1_bang()
   self:outlet(1, "float", {self.counter})
   self.counter = self.counter + self.step
end
~~~

That was easy enough. If you've been following along, you also know by now how to reload the patch and add a few bits to test the new features. For instance: 

![Creating arguments](04-creation-args.png)

## Log messages and errors

As soon as your objects get more complicated, you'll probably want to add some messages indicating to the user (or yourself) what's going on inside the object's methods. To these ends, Pd-Lua provides the following two facilities which let you output text messages to the Pd console:

- `pd.post(msg)` outputs the string `msg` to the console on a separate line. You can also post multi-line messages by embedding newline (`\n`) characters in the `msg` string. This is also frequently used for debugging purposes, e.g., to print out incoming messages or intermediate values that your code calculates.

- `self:error(msg)` reports an error message, given as a string `msg`, to the console. These messages are printed in red, to make them stand out, and you can use the "Find Last Error" menu option to locate the object which reported the error. (In Purr Data it's also possible to just click on the "error" link in the console to locate the object.) Note that `self:error` simply prints the message in a way that ties in with "Find Last Error". It doesn't abort the method that executes it, or have any other grave consequences. Thus you can also use it for debugging purposes, like `pd.post`, if you need to trace the message back to the object it came from.

For instance, let's use these facilities to have our `foo` object post the initial counter value in the `initialize` method, as well as report an error if any of the given creation arguments is of the wrong type. Here is the suitably modified `initialize` method:

~~~lua
function foo:initialize(sel, atoms)
   self.inlets = 1
   self.outlets = 1
   self.counter = 0
   self.step = 1
   if type(atoms[1]) == "number" then
      self.counter = atoms[1]
   elseif type(atoms[1]) ~= "nil" then
      self:error(string.format("foo: #1: %s is of the wrong type %s",
                               tostring(atoms[1]), type(atoms[1])))
   end
   if type(atoms[2]) == "number" then
      self.step = atoms[2]
   elseif type(atoms[2]) ~= "nil" then
      self:error(string.format("foo: #2: %s is of the wrong type %s",
                               tostring(atoms[2]), type(atoms[2])))
   end
   pd.post(string.format("foo: initialized counter: %g, step size: %g",
                         self.counter, self.step))
   return true
end
~~~

And here's how the console log looks like after loading our test patch, and creating an erroneous `foo bad` object:

![Log messages](05-post-and-error.png)

Note that the `foo bad` object was still created with the appropriate defaults *after* the error message, so the `initialize` method ran through to the end alright. If you *want* the object creation to fail after printing the error message, you only have to add a `return false` statement in the `elseif` branch, after the call to `self:error`. Try it! (Of course, you won't be able to locate the object using the printed error message in this case, since the object wasn't actually created. But "Find Last Error" will still work, since Pd itself will also print a "couldn't create" error message.)

Here's another fun exercise: Let's have `foo` print a welcome message when it first gets invoked. This can be done by adding a variable `init` to the `foo` class itself, which is shared between different object instances, as follows:

~~~lua
foo.init = false
~~~

You should put this after the definition of `foo` (i.e., after the line with the `pd.Class:new()` call), but before any code that uses this variable. Note that we could also just have used an ordinary local variable at script level instead, but this illustrates how you create static class members in Lua.

You still have to add the code which outputs the welcome message. An obvious place for this is somewhere in `initialize`, but here we use the `postinitialize` method for illustration purposes:

~~~lua
function foo:postinitialize()
   if not foo.init then
      pd.post("Welcome to foo! Copyright (c) by Foo software.")
      foo.init = true
   end
end
~~~

This will print the message just once, right after the first `foo` object is created. There's another `finalize` method which can be used to perform any kind of cleanup when an object gets destroyed. For instance, let's rework our example so that it keeps track of the actual number of `foo` objects, and prints an additional message when the last `foo` object is deleted. To these ends, we turn `foo.init` into a counter which keeps track of the number of `foo` objects:

~~~lua
foo.init = 0

function foo:postinitialize()
   if foo.init == 0 then
      pd.post("Welcome to foo! Copyright (c) by Foo software.")
   end
   foo.init = foo.init + 1
end

function foo:finalize()
   foo.init = foo.init - 1
   if foo.init == 0 then
      pd.post("Thanks for using foo!")
   end
end
~~~

Here are the messages logged in the console if we now load our test patch and then go on to delete all `foo` objects in it:

~~~
foo: initialized counter: 0, step size: 1
Welcome to foo! Copyright (c) by Foo software.
foo: initialized counter: 5, step size: 1
foo: initialized counter: 0, step size: 5
Thanks for using foo!
~~~

### Lua errors

We all make mistakes. It's inevitable that you'll run into errors in the Lua code you wrote, so let's finally discuss how those mishaps are handled. Pd-Lua simply reports errors from the Lua interpreter in the Pd console. For instance, suppose that we mistyped `pd.post` as `pd_post` in the code for the one-time welcome message above. You'll see an error message like this in the console:

~~~
error: pdlua_new: error in constructor for `foo':
[string "foo"]:7: attempt to call a nil value (global 'pd_post')
error: couldn't create "foo"
~~~

In this case the error happened in the `initialize` method, so the object couldn't actually be created, and you will have to correct the typo before going on. Fortunately, the message tells us exactly where the error occurred, so we can fix it easily. Syntax errors anywhere in the script file will be caught and handled in a similar fashion.

Runtime errors in inlet methods, on the other hand, will allow your objects to be created and to start executing; they just won't behave as expected and cause somewhat cryptic errors to be printed in the console. For instance, let's suppose that you forgot the curly braces around the float value in `self:outlet` (a fairly common error), so that the method reads:

~~~lua
function foo:in_1_bang()
   self:outlet(1, "float", self.counter) -- WRONG!
   self.counter = self.counter + self.step
end
~~~

Lua is a dynamically-typed language, so this little glitch becomes apparent only when you actually send a bang message to the object, which causes the following errors to be logged in the console:

~~~
error: lua: error: not a table
... click the link above to track it down, or click the 'Find Last Error' item in the Edit menu.
error: lua: error: no atoms??
~~~

Ok, so the first message tells us that *somewhere* Pd-Lua expected a table but got a non-table value. The second message actually comes from the C routine deep down in the bowls of Pd-Lua which does the actual output to an outlet. If you see that message, it's a telltale sign that you tried to output an atom not properly wrapped in a Lua table, but it gives no indication of where that happened either, other than that you can use "Find Last Error" to locate the object which caused the problem.

It goes without saying that the Pd-Lua developers could have chosen a better error message there. Well, at least we now have an idea what happened and in which object, but we may then still have to start peppering our code with `pd.post` calls in order to find (and fix) the issue.

## Inlets and outlets

As we've already seen, the number of inlets and outlets is set with the `inlets` and `outlets` member variables in the `initialize` method of an object. You can set these to any numbers you want, including zero. (In the current implementation, fractional numbers will be truncated to integers, and negative numbers will be treated as zero. If the variables aren't set at all, they also default to zero.)

### Inlets

Let's have a look at the inlets first. Pd-Lua supports a number of different forms of inlet methods which enable us to process any kind of Pd message. In the following list, "1" stands for any literal inlet number (counting the inlets from left to right, starting at 1), and "sym" for any symbol denoting either one of the predefined Pd message types (bang, float, symbol, pointer, and list), or any other (selector) symbol at the beginning of a Pd meta message. Note that, as usual, in your code these methods are always prefixed with the class name, using Lua colon syntax.

- `in_1_sym(...)` matches the given type or selector symbol on the given inlet; the method receives zero or one arguments (denoted `...` here), depending on the selector symbol `sym`, see below

- `in_n_sym(n, ...)` (with a verbatim "n" replacing the inlet number) matches the given type or selector symbol on *any* inlet; the actual inlet number is passed as the first argument (denoted `n` here), along with the zero or one extra arguments `...` which, like above, depend on the selector symbol `sym`

- `in_1(sel, atoms)` matches *any* type or selector symbol on the given inlet; the type or selector symbol is passed as a string `sel`, and the remaining arguments of the message are passed as a Lua table `atoms` containing number and string values

- `in_n(n, sel, atoms)` matches *any* type or selector symbol on *any* inlet; the method is invoked with the inlet number `n` along with type/selector symbol `sel` and the remaining message arguments in the Lua table `atoms`

These alternatives are tried in the indicated order, i.e., from most specific to most general. In addition, Pd-Lua understands the following specific `sym` type selectors and adjusts the number and type of the extra `...` arguments accordingly:

- `bang` denotes a bang message and passes no arguments

- `float` denotes a Pd float value, which is passed as a number argument

- `symbol` denotes a Pd symbol, which is passed as a string argument

- `pointer` denotes a Pd pointer, which is passed as a Lua userdata argument

- `list` denotes a Pd list, which is passed as a Lua table argument containing all the list elements

- any other `sym` value is taken as a literal Pd symbol to be matched against the selector symbol of the incoming message; the remaining arguments of the message are passed as a Lua table argument

Note that there can only be zero or one additional arguments in this case (besides the inlet number for `in_n_sym`). In contrast, the two most generic kinds of methods, `in_1` and `in_n`, always have the type/selector symbol `sel` (a string) and the remaining message arguments `atoms` (a Lua table) as arguments.

Among these, the methods for `bang`, `float`, and `list` are probably the most frequently used, along with `in_1` or `in_n` as a catch-all method for processing any other kind of input message. We've already employed the `in_1_bang` method in our basic example above. Here are some (rather contrived) examples for the other methods; we'll see some real examples of some of these later on.

~~~lua
function foo:in_1_float(x)
   pd.post(string.format("foo: got float %g", x))
end

function foo:in_1_symbol(x)
   pd.post(string.format("foo: got symbol %s", x))
end

function foo:in_1_list(x)
   pd.post(string.format("foo: got list %s", table.concat(x, " ")))
end

function foo:in_1_bar(x)
   pd.post(string.format("foo: got bar %s", table.concat(x, " ")))
end

function foo:in_n_baz(n, x)
   pd.post(string.format("foo: got baz %s on inlet #%d",
                         table.concat(x, " "), n))
end

function foo:in_1(sel, atoms)
   pd.post(string.format("foo: got %s %s", sel, table.concat(atoms, " ")))
end

function foo:in_n(n, sel, atoms)
   pd.post(string.format("foo: got %s %s on inlet #%d", sel,
                         table.concat(atoms, " "), n))
end
~~~

(Note that we omitted the pointer type in the above examples, as it is rarely used in Lua externals. But if you want, you can also receive such values, which will be represented as "userdata" a.k.a. C pointers on the Lua side. In Lua 5.4 it is possible to print such values using the `%p` format specifier of `string.format`, while in older Lua versions you will have to use the Lua `tostring()` function for this purpose.)

### Outlets

Luckily, things are much simpler on the output side. As we've already seen, to output a message to an outlet, you simply call `self:outlet(n, sel, atoms)` with the following arguments:

- `n` is the outlet number, counting from left to right, starting at 1

- `sel` is the type or selector symbol of the message; all the usual Pd type symbols that we've already seen above are recognized here as well: `bang`, `float`, `symbol`, `pointer`, `list`

- `atoms` are the remaining arguments of the message as a Lua table containing numbers, pointers and strings, as required by the message

Here are some common examples:

~~~lua
self:outlet(1, "bang", {})
self:outlet(1, "float", {math.pi})
self:outlet(1, "symbol", {"bar"})
self:outlet(1, "list", {1, 2, 3})
self:outlet(1, "fruit", {"apple", "orange", "kiwi"})
~~~

Usually, `self:outlet` will be called in the inlet methods of an object, but you'll also see it in clocks and receivers, which we'll discuss later.

Note that, by convention, most Pd objects handle inlets and outlets in a certain order, namely:

- The *leftmost* inlet is the so-called *hot* inlet which triggers the actual computation and resulting output of an object. Thus in Lua the calls to `self:outlet` should normally be put into the `in_1` methods.

- Consequently, outlets are normally triggered *from right to left*, so that, with a straight (non-crossing) wiring of the patch cables, a connected object gets its hot inlet triggered *last*. In Lua this means that your `self:outlet` calls should be ordered such that the outlet numbers are *decreasing*, not increasing with each call.

You'll also see this guideline being used in the Fibonacci number example in the next section. Let us emphasize again that this is merely a *convention* and thus you're not obliged to follow it, but most built-in and external Pd objects do. Thus if your Lua object works differently for no good reason, then seasoned Pd users will think that it is malfunctioning. There are some rare cases, however, where it's legitimate to deviate from these rules. Consider, for instance, the built-in `timer` object whose *right* inlet is the "hot" one.

## Fibonacci number example

Nobody in their right mind would actually bother to implement counters in Lua, since they're very easy to do directly in Pd. So let's now take a look at a slightly more interesting example, the Fibonacci numbers. It is also instructive to see how surprisingly difficult it is to write this as a Pd abstraction (you should actually give it a try), whereas it is really dead-easy in Lua.

If you know some math or have studied the Golden ratio, then you've probably heard about these. Starting from the pair 0, 1, the next number is always the sum of the two preceding ones: 0, 1, 1, 2, 3, 5, 8, 13, 21, 34, 55, 89, 144, etc. It goes without saying that these numbers grow pretty quickly (with the ratio of successive numbers approaching the Golden ratio). Thus we may want to limit their range, which is also useful if we want to use these numbers in a musical context, e.g., employing them as the basis of MIDI note numbers. One idea which produces both mathematically and musically interesting results is to take the numbers modulo *m*, i.e., just retain their remainders when divided by the given modulus. As these sequences all have a finite range, they must repeat eventually, but they have a surprisingly large period (also known as the *Pisano period* in number theory) even for small values of *m*.

So, without any further ado, here is a Pd-Lua object which calculates the Fibonacci numbers for a given modulus (10 by default, which, as [Wikipedia](https://en.wikipedia.org/wiki/Pisano_period) will tell you, has a Pisano period of 60). We actually compute (and output) the numbers in pairs, since we have to keep track of the pairs anyway in order to compute them efficiently.

~~~lua
local fibs = pd.Class:new():register("fibs")

function fibs:initialize(sel, atoms)
   -- one inlet for bangs and other messages
   self.inlets = 1
   -- two outlets for the numbers in pairs
   self.outlets = 2
   -- intial pair
   self.a, self.b = 0, 1
   -- the modulus can also be set as creation argument
   self.m = type(atoms[1]) == "number" and atoms[1] or 10
   -- make sure that it's an integer > 0
   self.m = math.max(1, math.floor(self.m))
   -- print the modulus in the console, so that the user knows what it is
   pd.post(string.format("fibs: modulus %d", self.m))
   return true
end

function fibs:in_1_bang()
   -- output the current pair in the conventional right-to-left order
   self:outlet(2, "float", {self.b})
   self:outlet(1, "float", {self.a})
   -- calculate the next pair; note that it's sufficient to calculate the
   -- remainder for the new number
   self.a, self.b = self.b, (self.a+self.b) % self.m
end

function fibs:in_1_float(m)
   -- a float input changes the modulus and resets the sequence
   self.m = math.max(1, math.floor(m))
   self.a, self.b = 0, 1
   pd.post(string.format("fibs: modulus %d", self.m))
end

function fibs:in_1_reset()
   -- a reset message just resets the sequence
   self.a, self.b = 0, 1
end
~~~

And here you can see the object running in a little test patch which outputs the two streams of Fibonacci notes to two different MIDI channels. The two streams can be enabled and disabled individually with the corresponding spigots, and you can also change the modulus on the fly.

![Fibonacci example](06-fibonacci.png)

## Using arrays and tables

Pd's arrays provide an efficient means to store possibly large vectors of float values. These are often used for sample data (waveforms) of a given size (the number of samples), but can also be employed to store copious amounts of numerical control data. Arrays are usually associated with a graphical display (called a *graph*), and Pd's table object lets you create an array along with a graph as a special kind of subpatch.

While Pd-Lua cannot currently process audio data in real-time, it does provide a `Table` class to represent array and table data, and a few functions to query and manipulate that data. This comes in handy, e.g., if you want to fill an array with a computed waveform. While Pd has its own corresponding facilities, complicated waveforms are often much easier to create in Lua, which offers a fairly complete set of basic mathematical functions in its standard library, and a whole lot more through 3rd party libraries such as [Numeric Lua](http://numlua.luaforge.net/).

Here are the array/table functions provided by Pd-Lua. Note that like in Pd arrays, indices are zero-based and thus range from `0` to `tab:length()-1`.

- `pd.Table:new():sync(name)`: creates the Lua representation of a Pd array and associates it with the Pd array named `name`. The result is `nil` if an array or table of that name doesn't exist. You usually assign that value to a local variable (named `tab` below) to refer to it later.

- `tab:length()`: returns the length of `tab` (i.e., the number of samples in it)

- `tab:get(i)`: gets the sample at index `i` from `tab`; returns a number, or `nil` if the index runs past the table boundaries

- `tab:set(i, x)`: sets the sample at index `i` of `tab` to `x` (a number)

- `tab:redraw()`: redraws the graph of `tab`; you should call this once you're finished updating the table

One important point worth mentioning here is that arrays and tables are subject to change at any time in Pd, as they may have their properties changed, be deleted, and recreated with new parameters. This means that you will have to call `pd.Table:new():sync(name)`, assign it to a local variable, and check that value *every* time you want to access the Pd array in a method.

Here is a simple example of a `luatab` object which takes the array name as a creation argument, and generates a waveform of the given frequency whenever a float value is received on the single inlet. After finishing generating the waveform, a bang message is output on the single outlet.

~~~lua
local luatab = pd.Class:new():register("luatab")

function luatab:initialize(sel, atoms)
   -- single inlet for the frequency, bang goes to the single outlet when we
   -- finished generating a new waveform
   self.inlets = 1
   self.outlets = 1
   -- the name of the array/table should be in the 1st creation argument
   if type(atoms[1]) == "string" then
      self.tabname = atoms[1]
      return true
   else
      self:error(string.format("luatab: expected array name, got %s",
                               tostring(atoms[1])))
      return false
   end
end

function luatab:in_1_float(freq)
   if type(freq) == "number" then
      -- the waveform we want to compute, adjust this as needed
      local function f(x)
         return math.sin(2*math.pi*freq*(x+1))/(x+1)
      end
      -- get the Pd array and its length
      local t = pd.Table:new():sync(self.tabname)
      if t == nil then
         self:error(string.format("luatab: array or table %s not found",
                                  self.tabname))
         return
      end
      local l = t:length()
      -- Pd array indices are zero-based
      for i = 0, l-1 do
         -- normalize arguments to the 0-1 range
         t:set(i, f(i/l))
      end
      -- this is needed to update the graph display
      t:redraw()
      -- output a bang to indicate that we've generated a new waveform
      self:outlet(1, "bang", {})
   else
      self:error(string.format("luatab: expected frequency, got %s",
                               tostring(freq)))
   end
end
~~~

And here is a sample patch running the `luatab` object:

![Table example](07-table.png)

In the same vein, the Pd-Lua distribution includes a much more comprehensive example ltabfill.pd_lua, which leverages Lua's `load` function to create a waveform from a user-specified Lua function created dynamically at runtime (instead of being hard-coded into the Lua code, which is what we did above).

## Using clocks

Clocks are used internally in Pd to implement objects which "do things" when a timeout occurs, such as delays, pipes, and metronomes. Pd-Lua exposes this functionality so that objects written in Lua can do the same. The following functions are provided:

- `pd.Clock:new():register(self, method)`: This creates a new clock for the Pd-Lua object `self` which, when it goes off, runs the method specified as a string `method`. Let's say that `method` is `"trigger"`, then `self:trigger()` will be called without arguments when the clock times out. You usually want to assign the result (a `pd.Clock` object) to a member variable of the object (called `self.clock` below), so that you can refer to it later.

- `self.clock:delay(time)`: sets the clock so that it will go off (and call the clock method) after `time` milliseconds

- `self.clock:set(systime)`: sets the clock so that it will go off at the specified absolute `systime` (measured in Pd "ticks", whatever that means)

- `self.clock:unset()`: unsets the clock, canceling any timeout that has been set previously

- `self.clock:destruct()`: destroys the clock; this is to be called in the `finalize` method, so that the clock doesn't go off (trying to invoke an invalid object) after an object was deleted

We mention in passing that you can call `self.clock:delay(time)` as soon as the clock has been created, even in the `initialize` method of an object. Furthermore, you can have as many clocks as you want in the same object, carrying out different actions, as long as you assign each clock to a different method.

Presumably, the `self.clock:destruct()` method should also be invoked automatically when setting `self.clock` to `nil`, but the available documentation isn't terribly clear on this. So we recommend explicitly calling `self.clock:destruct()` in `self:finalize` to be on the safe side, as the documentation advises us to do, because otherwise "weird things will happen."

Also note that `self.clock:set()` isn't terribly useful right now, because it refers to Pd's internal "systime" clock which isn't readily available in Pd-Lua.

With these caveats in mind, here is a little `tictoc` object we came up with for illustration purposes, along with the usual test patch.

~~~lua
local tictoc = pd.Class:new():register("tictoc")

function tictoc:initialize(sel, atoms)
   -- inlet 1 takes an on/off flag, inlet 2 the delay time
   self.inlets = 2
   -- bangs are output alternating between the two outlets
   self.outlets = 2
   -- the delay time (optional creation argument, 1000 msec by default)
   self.delay = type(atoms[1]) == "number" and atoms[1] or 1000
   -- we start out on the left outlet
   self.left = true
   -- initialize the clock
   self.clock = pd.Clock:new():register(self, "tictoc")
   return true
end

-- don't forget this, or else...

function tictoc:finalize()
  self.clock:destruct()
end

-- As with the metro object, nonzero, "bang" and "start" start the clock,
-- zero and "stop" stop it.

function tictoc:in_1_float(state)
   if state ~= 0 then
      -- output the first tick immediately
      self:tictoc()
   else
      -- stop the clock
      self.clock:unset()
   end
end

function tictoc:in_1_bang()
   self:in_1_float(1)
end

function tictoc:in_1_start()
   self:in_1_float(1)
end

function tictoc:in_1_stop()
   self:in_1_float(0)
end

-- set the delay (always in msec, we don't convert units)

function tictoc:in_2_float(delay)
   -- this will be picked up the next time the clock reschedules itself
   self.delay = delay >= 1 and delay or 1
end

-- the clock method: tic, toc, tic, toc ...

function tictoc:tictoc()
   -- output a bang, alternate between left and right
   self:outlet(self.left and 1 or 2, "bang", {})
   self.left = not self.left
   -- reschedule
   self.clock:delay(self.delay)
end
~~~

![Clock example](08-clock.png)

More comprehensive examples using clocks can be found in the Pd-Lua distribution; have a look, e.g., at ldelay.pd_lua and luametro.pd_lua. Also, lpipe.pd_lua demonstrates how to dynamically create an entire collection of clocks in order to implement a delay line for a stream of messages.

## Using receivers

As every seasoned Pd user knows, Pd also enables you to transmit messages in a wireless fashion, using receiver symbols, or just *receivers* for short, as destination addresses. In Pd, this is done through the built-in `send` and `receive` objects, as well as the "send" and "receive symbol" properties of GUI objects.

Sending messages to a receiver in Pd-Lua is straightforward:

- `pd.send(sym, sel, atoms)`: Sends a message with the given selector symbol `sel` (a string) and arguments `atoms` (a Lua table, which may be empty if the message has no arguments) to the given receiver `sym` (a string).

This works pretty much like the `outlet` method, but outputs messages to the given receiver instead. For instance, let's say you have a toggle with receiver symbol `onoff` in your patch, then you can turn on that toggle with a call like `pd.send("onoff", "float", {1})`. (Recall that the `atoms` argument always needs to be a table, even if it is a singleton, lest you'll get that dreaded "no atoms??" error that we discussed earlier).

One complication are receiver symbols using a `$0-` patch id prefix, which are commonly used to differentiate receiver symbols in different toplevel patches or abstractions, in order to prevent name clashes. A Pd-Lua object doesn't have any idea of what toplevel patch it is located in, and what the numeric id of that patch is, so you'll have to expand the `$0-` prefix on the Pd side and pass it, e.g., as a creation argument. For instance, suppose that the toggle receiver is in fact named `$0-onoff`, then something like the following Pd-Lua object will do the trick, if you invoke it as `luasend $0-onoff`:

~~~lua
local luasend = pd.Class:new():register("luasend")

function luasend:initialize(sel, atoms)
   self.inlets = 1
   self.receiver = tostring(atoms[1])
   return true
end

function luasend:in_1_bang()
   pd.send(self.receiver, "float", {1})
end
~~~

Of course, this also handles ordinary receive symbols just fine if you pass them as a creation argument. Here is a little test patch showing `luasend` in action:

![Send example](09-send.png)

It is worth noting here that the same technique applies whenever you need to pass on "$" arguments to a Pd-Lua object in a Pd abstraction.

So let's have a look at receivers now. These work pretty much like clocks in that you create them registering a method, and destroy them when they are no longer needed:

- `pd.Receive:new():register(self, sym, method)`: This creates a new receiver named `sym` (a string) for the Pd-Lua object `self` which, when a message for that receiver becomes available, runs the method specified as a string `method`. Let's say that `method` is `"receive"`, then `self:receive(sel, atoms)` will be invoked with the selector symbol `sel` and arguments `atoms` of the transmitted message. You want to assign the result (a `pd.Receive` object) to a member variable of the object (called `self.recv` below), so that you can refer to it later (if only to destroy it, see below).

- `self.recv:destruct()`: destroys the receiver

Note that the same caveat applies to receivers as in the case of clocks. That is, you should use the `destruct` method to destroy receivers in the `finalize` routine of the receiving object, so that they don't hang around when their object is long dead. Otherwise, you guessed it, "weird things will happen."

Here is a little example which receives any kind of message, stores it, and outputs the last stored message when it gets a `bang` on its inlet.

~~~lua
local luarecv = pd.Class:new():register("luarecv")

function luarecv:initialize(sel, atoms)
   self.inlets = 1
   self.outlets = 1
   -- pass the receiver symbol as creation argument
   local sym = tostring(atoms[1])
   pd.post(string.format("luarecv: receiver '%s'", sym))
   -- create the receiver
   self.recv = pd.Receive:new():register(self, sym, "receive")
   return true
end

function luarecv:finalize()
   self.recv:destruct()
end

function luarecv:receive(sel, atoms)
   -- simply store the message, so that we can output it later
   self.sel, self.atoms = sel, atoms
   pd.post(string.format("luarecv: got '%s %s'", sel,
                         table.concat(atoms, " ")))
end

function luarecv:in_1_bang()
   -- output the last message we received (if any)
   if self.sel then
      self:outlet(1, self.sel, self.atoms)
   end
end
~~~

The obligatory test patch:

![Receive example](10-receive.png)

## Live coding

I've been telling you all along that in order to make Pd-Lua pick up changes you made to your .pd_lua files, you have to relaunch Pd and reload your patches. Well, in this section we are going to discuss Pd-Lua's *live coding* features, which let you modify your sources and have Pd-Lua reload them on the fly, without ever exiting the Pd environment. This rapid incremental style of development is one of the hallmark features of dynamic programming environments like Pd and Lua. Musicians also like to employ it to modify their algorithmic composition programs live on stage, which is where the term "live coding" comes from. You'll probably be using live coding a lot while developing your Pd-Lua externals, but I've kept this topic for the final section of this guide, because it requires a good understanding of Pd-Lua's basic features. So without any further ado, let's dive right into it now.

First, we need to describe the predefined Pd-Lua object classes `pdlua` and `pdluax`, so that you know what's readily available. But we'll also discuss how to add a `reload` message to your existing object definitions. This is quite easy to do by directly employing Pd-Lua's `dofile` method, which is also what both `pdlua` and `pdluax` use internally.

### pdlua

The `pdlua` object accepts a single kind of message of the form `load filename` on its single inlet, which causes the given Lua file to be loaded and executed. Since `pdlua` has no outlets, its uses are rather limited. However, it does enable you to load global Lua definitions and execute an arbitrary number of statements, e.g., to post some text to the console or transmit messages to Pd receivers using the corresponding Pd-Lua functions. For instance, here's a little Lua script loadtest.lua which simply increments a global `counter` variable (first initializing it to zero if the variable doesn't exist yet) and posts its current value in the console:

~~~lua
counter = counter and counter + 1 or 0
pd.post(string.format("loadtest: counter = %d", counter))
~~~

To run this Lua code in Pd, you just need to connect the message `load loadtest.lua` to `pdlua`'s inlet (note that you really need to specify the full filename here, there's no default suffix):

![pdlua example](11-pdlua.png)

Now, each time you click on the `load loadtest.lua` message, the file is reloaded and executed, resulting in some output in the console, e.g.:

~~~
loadtest: counter = 0
loadtest: counter = 1
~~~

Also, you can edit the script between invocations and the new code will be loaded and used immediately. E.g., if you change `counter + 1` to `counter - 1`, you'll get:

~~~
loadtest: counter = 0
loadtest: counter = -1
~~~

That's about all there is to say about `pdlua`; it's a very simple object.

### pdluax

`pdluax` is a bit more elaborate and allows you to create real Pd-Lua objects with an arbitrary number of inlets, outlets, and methods. To these ends, it takes a single creation argument, the basename of a .pd_luax file. This file is a Lua script returning a function to be executed in `pdluax`'s own `initialize` method, which contains all the usual definitions, including the object's method definitions, in its body. This function receives the object's `self` as well as all the extra arguments `pdluax` was invoked with, and should return `true` if creation of the object succeeded.

For instance, here's a simplified version of our `foo` counter object, rewritten as a .pd_luax file, to be named foo.pd_luax:

~~~lua
return function (self, sel, atoms)
   self.inlets = 1
   self.outlets = 1
   self.counter = type(atoms[1]) == "number" and atoms[1] or 0
   self.step = type(atoms[2]) == "number" and atoms[2] or 1
   function self:in_1_bang()
      self:outlet(1, "float", {self.counter})
      self.counter = self.counter + self.step
   end
   return true
end
~~~

Note the colon syntax `self:in_1_bang()`. This adds the bang method directly to the `self` object rather than its class, which is `pdluax`. (We obviously don't want to modify the class itself, which may be used to create any number of different kinds of objects, each with their own collection of methods.) Also note that the outer function is "anonymous" (nameless) here; you can name it, but there's usually no need to do that, because this function will be executed just once, when the corresponding `pdluax` object is created. Another interesting point to mention here is that this approach of including all the object's method definitions in its initialization method works with regular .pd_lua objects, too; try it!

In the patch, we invoke a .pd_luax object by specifying the basename of its script file  as `pdluax`'s first argument, adding any additional creation arguments that the object itself might need:

![pdluax example](12-pdluax.png)

These `pdluax foo` objects work just the same as their regular `foo` counterparts, but there's an important difference: The code in foo.pd_luax is loaded *every time* you create a new `pdluax foo` object. Thus you can easily modify that file and just add a new `pdluax foo` object to have it run the latest version of your code. For instance, in foo.pd_luax take the line that reads:

~~~lua
      self.counter = self.counter + self.step
~~~

Now change that `+` operator to `-`:

~~~lua
      self.counter = self.counter - self.step
~~~

Don't forget to save your edits, then go back to the patch and recreate the `pdluax foo` object on the left. The quickest way to do that is to just delete the object, then use Pd's "Undo" operation, Ctrl+Z. Et voilà: the new object now decrements the counter rather than incrementing it. Also note that the other object on the right still runs the old code which increments the counter; thus you will have to give that object the same treatment if you want to update it, too.

While `pdluax` is considered Pd-Lua's main workhorse for live coding, it also has its quirks. Most notably, the syntax is different from regular object definitions, so you have to change the code if you want to turn it into a .pd_lua file. Also, having to recreate an object to reload the script file is quite disruptive (it resets the internal state of the object), and may leave objects in an inconsistent state (different objects may use various different versions of the same script file). Sometimes this may be what you want, but it makes `pdluax` somewhat difficult to use. It's not really tailored for interactive development, but it shines if you need a specialized tool for changing your objects on a whim in a live situation.

Fortunately, if you're not content with Pd-Lua's built-in facilities for live coding, it's easy to roll your own using the internal `dofile` method, which is discussed in the next subsection.

### dofile and dofilex

So let's discuss how to use `dofile` in a direct fashion. Actually, we're going to use its companion `dofilex` here, which works the same as `dofile`, but loads Lua code relative to the "externdir" of the class (the directory of the .pd_lua file) rather than the directory of the Pd canvas with the pdlua object, which is what `dofile` does. Normally, you don't have to worry about these intricacies, but they *will* matter if Lua class names are specified using relative pathnames, such as `../foo` or `bar/baz`. Since we're reloading class definitions here, it's better to use `dofilex` so that our code doesn't break if we later change the directory structure.

The method we sketch out below is really simple and doesn't have any of the drawbacks of the `pdluax` object, but you still have to add a small amount of boilerplate code to your existing object definition. Here is how `dofilex` is invoked:

- `self:dofilex(scriptname)`: This loads the given Lua script, like Lua's `loadfile`, but also performs a search on Pd's path to locate the file, and finally executes the file if it was loaded successfully. Note that `self` must be a valid Pd-Lua object, which is used solely to determine the externdir of the object's class, so that the script will be found if it is located there.

The return values of `dofilex` are those of the Lua script, along with the path under which the script was found. If the script itself returns no value, then only the path will be returned. (We don't use any of this information in what follows, but it may be useful in more elaborate schemes. For instance, `pdluax` uses the returned function to initialize the object, and the path in setting up the object's actual script name.)

Of course, `dofilex` needs the name of the script file to be loaded. We could hardcode this as a string literal, but it's easier to just ask the object itself for this information. Each Pd-Lua object has a number of private member variables, among them `_name` (which is the name under which the object class was registered) and `_scriptname` (which is the name of the corresponding script file, usually this is just `_name` with the `.pd_lua` extension tacked onto it). The latter is what we need. Pd-Lua also offers a `whoami()` method for this purpose, but that method just returns `_scriptname` if it is set, or `_name` otherwise. Regular Pd-Lua objects always have `_scriptname` set, so it will do for our purposes.

Finally, we need to decide how to invoke `dofilex` in our object. The easiest way to do this is to just add a message handler (i.e., an inlet method) to the object. For instance, say that the object is named `foo` which is defined in the foo.pd_lua script. Then all you have to do is add something like the following definition to the script:

~~~lua
function foo:in_1_reload()
   self:dofilex(self._scriptname)
end
~~~

As we already discussed, this code uses the object's internal `_scriptname` variable, and so is completely generic. You can just copy this over to any .pd_lua file, if you replace the `foo` prefix with whatever the name of your actual class variable is. With that definition added, you can now just send the object a `reload` message whenever you want to have its script file reloaded.

---

**NOTE:** This works because the `pd.Class:new():register("foo")` call of the object only registers a new class if that object class doesn't exist yet; otherwise it just returns the existing class.

By reloading the script file, all of the object's method definitions will be overwritten, not only for the object receiving the `reload` message, but for *all* objects of the same class, so it's sufficient to send the message to any (rather than every) object of the class. Also, existing object state (as embodied by the internal member variables of each object) will be preserved.

In general all this works pretty well, but there are some caveats, too. Note that if you *delete* one of the object's methods, or change its name, the old method will still hang around in the runtime class definition until you relaunch Pd. That's because reloading the script does not erase any old method definitions, it merely replaces existing and adds new ones.

Finally, keep in mind that reloading the script file does *not* re-execute the `initialize` method. This method is only invoked when an object is instantiated. Thus, in particular, reloading the file won't change the number of inlets and outlets of an existing object. Newly created objects *will* pick up the changes in `initialize`, though, and have the proper number of inlets and outlets if those member variables were changed.

---

Let's give this a try, using luatab.pd_lua from the "Using arrays and tables" section as an example. In fact, that's a perfect showcase for live coding, since we want to be able to change the definition of the waveform function `f` in `luatab:in_1_float` on the fly. Just add the following code to the end of luatab.pd_lua:

~~~lua
function luatab:in_1_reload()
   self:dofilex(self._scriptname)
end
~~~

Now launch the luatab.pd patch and connect a `reload` message to the `luatab wave` object, like so:

![Livecoding example 1](13-livecoding1.png)

Next change the wavetable function to whatever you want, e.g.:

~~~lua
      local function f(x)
         return math.sin(2*math.pi*freq*x)+1/3*math.sin(2*math.pi*3*freq*x)
      end
~~~

Return to the patch, click the `reload` message, and finally reenter the frequency value, so that the waveform gets updated:

![Livecoding example 2](14-livecoding2.png)

### Remote control

The method sketched out in the preceding subsection works well enough for simple patches. However, having to manually wire up the `reload` message to one object of each class that you're editing is still quite cumbersome. In a big patch, which is being changed all the time, this quickly becomes unwieldy. Wouldn't it be nice if we could equip each object with a special receiver, so that we can just click a message somewhere in the patch to reload a given class, or even all Pd-Lua objects at once? And maybe even do that remotely from the editor, using the `pdsend` program?

Well, all this is in fact possible, but the implementation is a bit too involved to fully present it here. So we have provided this in a separate pdx.lua module, which you can find in the sources accompanying this tutorial.

Setting up an object for this kind of remote control is easy, though. First, you need to import the `pdx` module into your script, using Lua's `require`:

~~~lua
local pdx = require 'pdx'
~~~

Then just call `pdx.reload(self)` somewhere in the `initialize` method. This will set up the whole receiver/dofilex machinery in a fully automatic manner. Finally, add a message like this to your patch, which goes to the special `pdluax` receiver (note that this is completely unrelated to the `pdluax` object discussed previously, it just incidentally uses the same name):

~~~
; pdluax reload
~~~

When clicked, this just reloads all Pd-Lua objects in the patch, provided they have been set up with `pdx.reload`. You can also specify the class to be reloaded (the receiver matches this against each object's class name):

~~~
; pdluax reload foo
~~~

Or maybe name several classes, like so:

~~~
; pdluax reload foo, reload bar
~~~

You get the idea. Getting set up for remote control via `pdsend` isn't much harder. E.g., let's say that we use UDP port 4711 on localhost for communicating with Pd, then you just need to connect `netreceive 4711 1` to the `; pdluax reload` message in a suitable way, e.g.:

![Remote control 1](15-remote-control1.png)

You can then use `pdsend 4711 localhost udp` to transmit the `reload` message to Pd when needed. You probably don't want to run those commands yourself, but a decent code editor will let you bind a keyboard command which does this for you. Myself, I'm a die-hard Emacs fan, so I've included a little elisp module pd-remote.el in the accompanying examples which shows how to do this. Once you've added this to your .emacs, you can just type Ctrl+C Ctrl+K in Emacs to make Pd reload your Lua script after saving it. It doesn't get much easier than that. Moreover, for your convenience I've added a little abstraction named pd-remote.pd which takes care of the `netreceive` and messaging bits and will look much tidier in your patches.

---

**NOTE:** To use these facilities in your own patches, you'll have to copy pdx.lua and pd-remote.pd to your project directory or some other place where Pd finds them. The pd-remote.el file can be installed in your Emacs site-lisp directory if needed. Please also check the [pd-remote](https://github.com/agraef/pd-remote) repository on GitHub for the latest pd-remote version and further details. This also includes a pointer to a Visual Studio Code extension written by Baris Altun which can be used as a replacement for pd-remote.el if you're not familiar with Emacs, or just prefer to use VS Code as an editor.

---

So here's the full source code of our reworked `luatab` example (now with the `in_1_reload` handler removed and the `pdx.reload` call added to the `initialize` method):

~~~lua
local luatab = pd.Class:new():register("luatab")

-- our own pdlua extension, needed for the reload functionality
local pdx = require 'pdx'

function luatab:initialize(sel, atoms)
   -- single inlet for the frequency, bang goes to the single outlet when we
   -- finished generating a new waveform
   self.inlets = 1
   self.outlets = 1
   -- enable the reload callback
   pdx.reload(self)
   -- the name of the array/table should be in the 1st creation argument
   if type(atoms[1]) == "string" then
      self.tabname = atoms[1]
      return true
   else
      self:error(string.format("luatab: expected array name, got %s",
                               tostring(atoms[1])))
      return false
   end
end

function luatab:in_1_float(freq)
   if type(freq) == "number" then
      -- the waveform we want to compute, adjust this as needed
      local function f(x)
         return math.sin(2*math.pi*freq*(x+1))/(x+1)
      end
      -- get the Pd array and its length
      local t = pd.Table:new():sync(self.tabname)
      if t == nil then
         self:error(string.format("luatab: array or table %s not found",
                                  self.tabname))
         return
      end
      local l = t:length()
      -- Pd array indices are zero-based
      for i = 0, l-1 do
         -- normalize arguments to the 0-1 range
         t:set(i, f(i/l))
      end
      -- this is needed to update the graph display
      t:redraw()
      -- output a bang to indicate that we've generated a new waveform
      self:outlet(1, "bang", {})
   else
      self:error(string.format("luatab: expected frequency, got %s",
                               tostring(freq)))
   end
end
~~~

And here's a little gif showing the above patch in action. You may want to watch this in [Typora](https://www.typora.io/) or your favorite web browser to make the animation work.

![Remote control 2](16-remote-control2.gif)

So there you have it: three (or rather four) different ways to live-code with Pd-Lua. Choose whatever best fits your purpose and is the most convenient for you.

## Conclusion

Congratulations! If you made it this far, you should have learned more than enough to start using Pd-Lua successfully for your own projects. You should also be able to read and understand the many examples in the Pd-Lua distribution, which illustrate all the various features in much more detail than we could muster in this introduction. You can find these in the examples folder, both in the Pd-Lua sources and the pdlua folder of your Pd installation.

The examples accompanying this tutorial (including the pdx.lua, pdlua-remote.el and pdlua-remote.pd files mentioned at the end of the previous section) are also available for your perusal in the examples subdirectory of the folder where you found this document.

Kudos to Claude Heiland-Allen for creating such an amazing tool, it makes programming Pd externals really easy and fun. Thanks are also due to Roberto Ierusalimschy for Lua, which for me is one of the best-designed, easiest to learn, and most capable multi-paradigm scripting languages there are today, while also being fast, simple, and light-weight.
