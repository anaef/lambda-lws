# Lua Web Services for AWS Lambda

Lua Web Services (LWS) provides a custom runtime for [AWS Lambda](https://aws.amazon.com/lambda/)
that supports web services written in Lua, running in a *serverless* computing environment.

Some central design considerations are the following:

- **Use [PUC-Lua](https://www.lua.org/).** PUC-Lua is the original implementation of Lua
maintained by the language's creators.

- **Efficiency**. The custom runtime is implemented in C and makes an effort to be efficient,
such as by using [yyjson](https://github.com/ibireme/yyjson) for JSON in-place parsing, in-place
Base64 processing, and generally avoiding the copying of data. Moreover, LWS reuses the Lua state
of the AWS Lambda execution environment for subsequent requests.

- **Configurability**. The custom runtime is typically deployed as a Docker image. A broad range
of [environment variables](doc/EnvironmentVariables.md) allows for configuration.

- **Focus on web services**. The main purpose of LWS is to implement web services in Lua. This
focus streamlines the design of LWS.

- **Mirror the request processing of [LWS for NGINX](https://github.com/anaef/nginx-lws)**. The
two projects are designed to be used together, such as with using the NGINX version for development
and the AWS Lambda version for staging and production environments.

*This project is not affiliated with, endorsed by, or sponsored by Amazon.com, Inc. (including
Amazon Web Services and AWS) or F5, Inc.*


## Discussion

This section briefly discusses the motivations for using PUC-Lua instead of LuaJIT and allowing
Lua web services to block instead of using an event-based, non-blocking architecture.

While [LuaJIT](https://luajit.org/) is undoubtedly an amazing feat of engineering with impressive
performance, it remains based on Lua 5.1 with select extensions. The latest PUC-Lua release is
5.4. PUC-Lua has added new language features over the years, including 64-bit integers, bit
operators, and variable attributes, which are not directly supported in LuaJIT. Perhaps more
worryingly, writing "fast" LuaJIT code promotes using language idioms that are amenable to its
optimization while eschewing language features that are "slow".[^1] It is not ideal if a JIT
compiler informs how a language is used.

In practice, the PUC-Lua VM is more than fast enough for a broad range of workloads. If its
performance is deemed insufficient for a particular function of a web service, implementing that
function in C is always possible. Furthermore, the research team at PUC-Rio is working on
ahead-of-time compilation through Pallene.[^2]


## Release Notes

Please see the [release notes](NEWS.md) document.


## Documentation

Please browse the extensive documentation in the [doc](doc) folder.

AWS Lambda documentation is found here:

* [Developer Guide](https://docs.aws.amazon.com/lambda/latest/dg/welcome.html)
* [Runtime API](https://docs.aws.amazon.com/lambda/latest/dg/runtimes-api.html)
* [Working with Lambda environment variables](https://docs.aws.amazon.com/lambda/latest/dg/configuration-envvars.html)
* [Invoking Lambda function URLs](https://docs.aws.amazon.com/lambda/latest/dg/urls-invocation.html)


## Limitations

LWS has been tested with the following software versions:

* Lua 5.4

Your mileage may vary with other software versions.


## Trademarks

Amazon Web Services, AWS, the Powered by AWS logo, and AWS Lambda are trademarks of Amazon.com,
Inc. or its affiliates in the United States and/or other countries.  

NGINX is a registered trademark of F5, Inc.

All other trademarks are the property of their respective owners.


## License

This project is licensed under the MIT License.
  
Some files (`src/lws_ngx.c`, `src/lws_ngx.h`) are derived from NGINX and carry their original
license terms. See [LICENSE](./LICENSE) for full details.


[^1]: Hugo Musso Gualandi. The Pallene Programming Language. 2020.
[Link](http://www.lua.inf.puc-rio.br/publications/2020-HugoGualandi-phd-thesis.pdf).

[^2]: Roberto Ierusalimschy. What about Pallene? Lua Workshop 2022. 2022.
[Link](https://www.lua.org/wshop22/Ierusalimschy.pdf).
