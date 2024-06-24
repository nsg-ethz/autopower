function makeAjaxForm(ajaxForm, successFun, errorFun) {
    ajaxForm.on("submit", (function(e) {
        e.preventDefault(); // do not submit via standard HTTP
        //if(ajaxForm.valid()) {
            ajaxForm.find(":submit").prop("disabled", true);
            $.ajax({
                type:"POST",
                url:$(this).attr('action'),
                data:$(this).serialize(),
                success: successFun,
                error: errorFun
            });
        //}
    }));
}