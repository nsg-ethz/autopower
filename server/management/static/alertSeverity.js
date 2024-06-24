
function setAlertSeverity(msg,alertSeverityClass) {
    msg.removeClass("alert-primary");
    msg.removeClass("alert-secondary");
    msg.removeClass("alert-info");
    msg.removeClass("alert-warning");
    msg.removeClass("alert-success");
    msg.removeClass("alert-danger");
    msg.addClass(alertSeverityClass);
}
